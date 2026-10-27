#!/usr/bin/env python3
"""frsim - FreeRTOS simulator for small embedded targets (no hardware needed).

  freertos  code using the FreeRTOS API, compiled with the real kernel
            (POSIX port: one pthread per task, SIGALRM as the tick), built
            32-bit (-m32) so pointers, StackType_t and heap blocks have MCU
            sizes. Simulated interrupts, GPIO, UART and ADC: sim/sim_hw.h.
  c         any C program, e.g. a bare-metal superloop or your own
            scheduler; timeline via sim/simtrace.h (st_run, st_block, ...)

  frsim.py run FILE [options]     run, print output, statistics and timeline
  frsim.py serve [--port 8765]    web IDE at http://127.0.0.1:8765

Options can also be set in the file:  // frsim: mcu=stm32f103c8 heap=4 duration=2
"""
import argparse
import bisect
import concurrent.futures
import glob
import hashlib
import http.server
import json
import math
import os
import re
import resource
import shutil
import signal
import subprocess
import sys
import tempfile
import threading
import time
import urllib.parse

ROOT = os.path.dirname(os.path.abspath(__file__))
SIM = os.path.join(ROOT, 'sim')
WEB = os.path.join(ROOT, 'web')
EXAMPLES = os.path.join(ROOT, 'examples')
BUILD = os.path.join(ROOT, 'build')
KERNEL = os.environ.get('FREERTOS_KERNEL',
                        os.path.join(os.path.dirname(ROOT), 'FreeRTOS-Kernel'))
PORT_DIR = os.path.join(KERNEL, 'portable', 'ThirdParty', 'GCC', 'Posix')
CC = os.environ.get('CC', 'gcc')

MAX_OUT = 1 << 20          # bytes of stdout/stderr returned
MAX_FILE = 64 << 20        # RLIMIT_FSIZE for the simulated program
MAX_INTERVALS = 200000
MAX_OBJ_EVENTS = 20000

# Target presets: RAM limit, default heap, bytes the port pushes on the task
# stack at a context switch (Cortex-M0/M3: 16 words; M4F: 17 words without
# an FPU frame), main-stack (MSP) reserve of a typical startup file.
MCUS = {
    'host':        {'name': 'Không giới hạn (máy tính)', 'core': '-', 'ram': None, 'heap': 65536, 'ctx': 64, 'msp': 0},
    'stm32f030f4': {'name': 'STM32F030F4 · Cortex-M0 · 48 MHz · 4 KB RAM', 'core': 'Cortex-M0', 'ram': 4096, 'heap': 2048, 'ctx': 64, 'msp': 512},
    'stm32f103c8': {'name': 'STM32F103C8 (Blue Pill) · Cortex-M3 · 72 MHz · 20 KB RAM', 'core': 'Cortex-M3', 'ram': 20480, 'heap': 10240, 'ctx': 64, 'msp': 1024},
    'stm32f411ce': {'name': 'STM32F411CE (Black Pill) · Cortex-M4F · 100 MHz · 128 KB RAM', 'core': 'Cortex-M4F', 'ram': 131072, 'heap': 65536, 'ctx': 68, 'msp': 1024},
    'rp2040':      {'name': 'RP2040 (1 core) · Cortex-M0+ · 133 MHz · 264 KB RAM', 'core': 'Cortex-M0+', 'ram': 270336, 'heap': 131072, 'ctx': 64, 'msp': 2048},
}

# name: (macro, type, default, allowed)
OPTIONS = {
    'mcu':               (None, str, 'host', tuple(MCUS)),
    'preemption':        ('configUSE_PREEMPTION', int, 1, (0, 1)),
    'time_slicing':      ('configUSE_TIME_SLICING', int, 1, (0, 1)),
    'idle_should_yield': ('configIDLE_SHOULD_YIELD', int, 1, (0, 1)),
    'tick_rate':         ('configTICK_RATE_HZ', int, 1000, (10, 1000)),
    'max_priorities':    ('configMAX_PRIORITIES', int, 7, (2, 32)),
    'static_alloc':      ('configSUPPORT_STATIC_ALLOCATION', int, 0, (0, 1)),
    'heap':              (None, int, 4, (1, 5)),
    'heap_size':         ('configTOTAL_HEAP_SIZE', int, 1 << 20, (512, 64 << 20)),   # ample: the run must not fail for memory
    'duration':          (None, float, 10.0, (0.05, 60.0)),
    'timestamp':         (None, int, 0, (0, 1)),
    'debug':             (None, int, 0, (0, 1)),
}

# Stack cost assumed for functions that are not compiled for the analysis.
PRINTF_STACK = 400     # newlib-nano printf on Cortex-M, order of magnitude
EXTERN_STACK = {f: PRINTF_STACK for f in (
    'sim_printf', 'sim_puts', 'sim_putchar', 'printf', 'puts', 'putchar', 'sprintf', 'snprintf',
    'vsnprintf', 'vPrintString', 'vPrintLine', 'vPrintStringAndNumber', 'vPrintLineAndNumber',
    'vPrintTwoStrings')}
SIM_ONLY = re.compile(r'^(vSim|ulSim|pvSim|lSim|xSim|__stack_chk)')   # simulator hooks, absent on the MCU
# Reached from task code only in theory: the tick hook runs in the tick ISR (main stack);
# xTaskIncrementTick() skips it while xTaskResumeAll() replays pended ticks.
ISR_ONLY = {'vApplicationTickHook'}
HOST_OBJS = ('port.c', 'wait_for_event.c', 'sim_support.c', 'sim_hw.c')

_build_lock = threading.Lock()
_run_lock = threading.Lock()


def normalize(opts):
    out = {}
    for name, (_, typ, default, allowed) in OPTIONS.items():
        v = opts.get(name, default) if opts else default
        try:
            v = typ(v)
        except (TypeError, ValueError):
            v = default
        if typ is str:
            out[name] = v if v in allowed else default
        else:
            out[name] = min(max(v, allowed[0]), allowed[1])
    return out


def directives(code):
    """Options written in the file: '// frsim: mcu=stm32f103c8 heap=4'."""
    d = {}
    for m in re.finditer(r'frsim:\s*([^\n*]*)', code):
        for k, v in re.findall(r'([a-z_]+)\s*=\s*([\w.]+)', m.group(1)):
            if k in OPTIONS:
                d[k] = v
    return d


def defines(o):
    d = ['-D%s=%d' % (OPTIONS[k][0], o[k]) for k in OPTIONS if OPTIONS[k][0]]
    d.append('-DSIM_HEAP_FREE_KNOWN=%d' % (0 if o['heap'] == 3 else 1))
    return d


def includes():
    return ['-I', SIM, '-I', os.path.join(KERNEL, 'include'),
            '-I', PORT_DIR, '-I', os.path.join(PORT_DIR, 'utils')]


_m32 = None


def m32_flags():
    """-m32 when the 32-bit libc is installed. Ubuntu without gcc-multilib
    lacks /usr/include/asm for i386; the x86 asm headers are bi-arch, so a
    symlink to the x86_64 ones is enough."""
    global _m32
    if _m32 is not None:
        return _m32
    os.makedirs(BUILD, exist_ok=True)
    flags = ['-m32']
    if not os.path.exists('/usr/include/asm') and os.path.isdir('/usr/include/x86_64-linux-gnu/asm'):
        shim = os.path.join(BUILD, 'inc32')
        os.makedirs(shim, exist_ok=True)
        link = os.path.join(shim, 'asm')
        if not os.path.islink(link):
            os.symlink('/usr/include/x86_64-linux-gnu/asm', link)
        flags += ['-idirafter', shim]
    with tempfile.TemporaryDirectory() as d:
        src = os.path.join(d, 't.c')
        with open(src, 'w') as f:
            f.write('#include <errno.h>\n#include <pthread.h>\nint main(void){return sizeof(void*)==4?0:1;}\n')
        r = subprocess.run([CC] + flags + [src, '-o', os.path.join(d, 't'), '-pthread'], capture_output=True)
        ok = r.returncode == 0 and subprocess.run([os.path.join(d, 't')]).returncode == 0
    _m32 = flags if ok else []
    return _m32


def kernel_sources(heap):
    s = [os.path.join(KERNEL, f) for f in
         ('tasks.c', 'queue.c', 'list.c', 'timers.c', 'event_groups.c', 'stream_buffer.c')]
    s += [os.path.join(PORT_DIR, 'port.c'),
          os.path.join(PORT_DIR, 'utils', 'wait_for_event.c'),
          os.path.join(KERNEL, 'portable', 'MemMang', 'heap_%d.c' % heap),
          os.path.join(SIM, 'sim_support.c'), os.path.join(SIM, 'sim_hw.c')]
    return s


# -Os as in a typical MCU release build; call graph + frame sizes for the stack estimate
ANALYSIS_FLAGS = ['-Os', '-fstack-usage', '-fcallgraph-info=su', '-fno-stack-protector', '-U_FORTIFY_SOURCE']


def build_kernel(o):
    """Compile kernel + support once per configuration; return (dir, lib, log)."""
    srcs = kernel_sources(o['heap'])
    flags = m32_flags() + ['-std=gnu11', '-g', '-pthread', '-w'] + ANALYSIS_FLAGS + includes() + defines(o)
    h = hashlib.sha1()
    h.update(' '.join(flags + [CC]).encode())
    for f in srcs + glob.glob(os.path.join(SIM, '*.h')):
        st = os.stat(f)
        h.update(('%s %d %d' % (f, st.st_size, st.st_mtime_ns)).encode())
    out = os.path.join(BUILD, 'k-' + h.hexdigest()[:12])
    lib = os.path.join(out, 'libfreertos.a')
    with _build_lock:
        if os.path.exists(lib):
            return out, lib, ''
        os.makedirs(out, exist_ok=True)

        def cc(src):
            obj = os.path.join(out, os.path.basename(src) + '.o')
            r = subprocess.run([CC, '-c', src, '-o', obj] + flags, capture_output=True, text=True, cwd=out)
            return obj, r.returncode, r.stderr

        with concurrent.futures.ThreadPoolExecutor(os.cpu_count() or 4) as ex:
            res = list(ex.map(cc, srcs))
        errs = ''.join(e for _, rc, e in res if rc)
        if errs:
            shutil.rmtree(out, ignore_errors=True)
            raise BuildError('kernel build failed:\n' + errs)
        subprocess.run(['ar', 'rcs', lib] + [obj for obj, _, _ in res], check=True)
        return out, lib, '[sim] built kernel for this configuration (%s, cached in %s)\n' % (
            '32-bit' if m32_flags() else '64-bit: -m32 unavailable, heap sizes larger than on an MCU', out)


class BuildError(Exception):
    pass


def _src_name(filename, default):
    base = os.path.basename(filename or default) or default
    return re.sub(r'[^A-Za-z0-9_.+-]', '_', base)


def compile_user(code, filename, o, workdir):
    """Return (exe or None, log, (kernel dir, analysis object or None))."""
    base = _src_name(filename, 'main.c')
    with open(os.path.join(workdir, base), 'w') as f:
        f.write(code)
    kdir, lib, log = build_kernel(o)
    extra = []
    if re.search(r'\bmainDELAY_LOOP_COUNT\b', code) and \
            not re.search(r'#\s*define\s+mainDELAY_LOOP_COUNT\b', code):
        extra.append('-DmainDELAY_LOOP_COUNT=0xffffff')
        log += '[sim] mainDELAY_LOOP_COUNT not defined in the file -> using 0xffffff (as in the book sources)\n'
    if 'vPortSetInterruptHandler' in code:     # book ISRs: uint32_t handler without return
        extra.append('-Wno-return-type')
    dbg = ['-fsanitize-coverage=trace-pc', '-no-pie'] if o['debug'] else []
    common = m32_flags() + ['-std=gnu11', '-g', '-pthread', '-Wall', '-Wno-unused-variable',
                            '-Wno-unused-but-set-variable', '-fdiagnostics-color=never'] + \
        includes() + defines(o) + extra + ['-include', os.path.join(SIM, 'sim_prelude.h')]
    exe = os.path.join(workdir, 'a.out')
    r = subprocess.run([CC, '-O0'] + common + dbg + ['-x', 'c', base, '-x', 'none', lib, '-o', exe, '-lrt'],
                       cwd=workdir, capture_output=True, text=True)
    log += r.stdout + r.stderr
    if r.returncode != 0:
        return None, log, None
    an = os.path.join(workdir, 'an', 'user.o')
    os.makedirs(os.path.dirname(an), exist_ok=True)
    ra = subprocess.run([CC, '-c', '-w'] + common + ANALYSIS_FLAGS + ['-x', 'c', base, '-o', an],
                        cwd=workdir, capture_output=True, text=True)
    return exe, log, (kdir, an if ra.returncode == 0 else None)


def compile_c(code, filename, workdir):
    base = _src_name(filename, 'main.c')
    with open(os.path.join(workdir, base), 'w') as f:
        f.write(code)
    exe = os.path.join(workdir, 'a.out')
    cmd = [CC, '-std=gnu11', '-O0', '-g', '-Wall', '-Wno-unused-variable', '-fdiagnostics-color=never',
           '-I', SIM, '-include', os.path.join(SIM, 'simtrace.h'),
           '-x', 'c', base, '-x', 'none', os.path.join(SIM, 'simtrace.c'), '-o', exe, '-lm', '-pthread']
    r = subprocess.run(cmd, cwd=workdir, capture_output=True, text=True)
    return (exe if r.returncode == 0 else None), r.stdout + r.stderr


def _limits(duration):
    def f():
        resource.setrlimit(resource.RLIMIT_FSIZE, (MAX_FILE, MAX_FILE))
        cpu = int(duration) + 10
        resource.setrlimit(resource.RLIMIT_CPU, (cpu, cpu))
        signal.signal(signal.SIGXFSZ, signal.SIG_IGN)
    return f


def _read_capped(path):
    with open(path, 'rb') as f:
        data = f.read(MAX_OUT + 1)
    text = data[:MAX_OUT].decode('utf-8', 'replace')
    if len(data) > MAX_OUT:
        text += '\n[sim] ... output truncated at %d KB\n' % (MAX_OUT >> 10)
    return text


def run_exe(exe, o, workdir, mode, code):
    trace = os.path.join(workdir, 'trace.txt')
    out, err = os.path.join(workdir, 'stdout'), os.path.join(workdir, 'stderr')
    env = dict(os.environ, SIM_TRACE=trace, SIM_TIMESTAMP=str(o['timestamp']), SIM_DEBUG=str(o.get('debug', 0)))
    with open(out, 'wb') as fo, open(err, 'wb') as fe:
        t0 = time.monotonic()
        p = subprocess.Popen([exe], cwd=workdir, stdout=fo, stderr=fe, stdin=subprocess.DEVNULL,
                             env=env, preexec_fn=_limits(o['duration']), start_new_session=True)
        try:
            rc = p.wait(timeout=o['duration'])
        except subprocess.TimeoutExpired:
            os.killpg(p.pid, signal.SIGKILL)
            p.wait()
            rc = None
        wall = time.monotonic() - t0
    if rc is None:
        reason = 'stopped after %.2f s' % o['duration']
        if mode == 'freertos':
            reason += ' (normal: a FreeRTOS scheduler never returns)'
    elif rc == 3 and mode == 'freertos':
        reason = 'configASSERT() failed (exit code 3)'
    elif rc == 4 and mode == 'freertos':
        reason = 'blocking API called inside an ISR (exit code 4)'
    elif rc < 0:
        reason = 'killed by signal %s' % signal.Signals(-rc).name
    else:
        reason = 'process exited with code %d' % rc
    tick = int(1e6 / o['tick_rate']) if mode == 'freertos' else None
    tr = parse_trace(trace, tick, code)
    if tr is not None and mode == 'freertos':
        tr.update({'heap_scheme': o['heap'], 'heap_size': o['heap_size']})
    dbg = None
    if o.get('debug') and mode == 'freertos':
        with open(out, 'rb') as f:
            dbg = debug_steps(trace, code, exe, os.path.basename(exe_src(workdir)), f.read())
    return {
        'debug': dbg,
        'stdout': _read_capped(out),
        'stderr': _read_capped(err),
        'exit': reason,
        'returncode': rc,
        'wall_s': round(wall, 3),
        'trace': tr,
    }


# ---------------------------------------------------------------- trace

OBJ_TYPES = {0: 'queue', 1: 'mutex', 2: 'counting', 3: 'binary', 4: 'recursive', 9: 'eventgroup'}
OBJ_LABEL = {'queue': 'Queue', 'mutex': 'Mutex', 'counting': 'Counting sem', 'binary': 'Binary sem',
             'recursive': 'Recursive mutex', 'eventgroup': 'Event group', 'timer': 'Timer'}
# number of numeric fields per event code; the rest of the line is a name
NFIELDS = {'C': 2, 'k': 3, 'I': 1, 'O': 2, 'R': 1, 'D': 1, 'P': 2, 'T': 1, 'M': 4, 'F': 4, 'V': 1, 'K': 0,
           'B': 3, 'Q': 4, 'N': 2, 'S': 3, 's': 3, 'G': 3, 'g': 3, 'c': 3, 'U': 2, 'A': 2, 'H': 2, 'h': 2,
           'Y': 3, 'X': 1, 'e': 3, 'x': 3, 'i': 1, 'j': 1, 'y': 1, 'J': 1, 'L': 2, 'l': 2, 'Z': 1}
CREATE_RE = re.compile(r'\bx(Queue|Semaphore|Timer|EventGroup)\w*Create\w*\s*\(')


def var_on_line(line):
    """Name of the variable a create call on this source line is assigned to."""
    m = CREATE_RE.search(line)
    if not m:
        return None
    lhs = re.findall(r'(\w+)\s*(?:\[[^\]]*\])?\s*=(?!=)', line[:m.start()])
    return lhs[-1] if lhs else None


def parse_trace(path, tick_us=None, code=''):
    """Turn trace events ("<time> <code> <numbers> [name]") into lanes.

    Tasks: C create, k stack/entry, I in, O out (1 = still ready), R ready,
    D deleted, P priority, H/h inherit/disinherit, B block reason, U suspend,
    A notified. Objects: Q create, N name/line, S/s send ok/full, G/g receive
    ok/empty, c initial count, Y timer create, X timer expired, e/x event bits
    set/clear. Interrupts: J attach, i/j enter/exit, y yield request.
    GPIO: l name, L level. Other: T tick, M/F heap, K marker, V virtual time,
    Z task API inside an ISR.
    """
    try:
        with open(path, errors='replace') as f:
            lines = f.read().splitlines()
    except OSError:
        return None
    src_lines = code.split('\n') if code else []
    evs = []
    vidx = None
    for line in lines:
        p = line.split(None, 2)
        if len(p) < 2 or not p[0].isdigit() or p[1] not in NFIELDS:
            continue
        evs.append((int(p[0]), p[1], p[2] if len(p) > 2 else ''))
        if p[1] == 'V' and vidx is None:
            vidx = len(evs) - 1
    virtual = vidx is not None
    if virtual:                     # events before st_time() carry real-time stamps
        tv = evs[vidx][0]
        evs = [(min(t, tv) if i < vidx else t, c, r) for i, (t, c, r) in enumerate(evs)]

    tasks, order = {}, []
    run, ready, blocked = {}, {}, {}
    ticks, heap, marks, misuse = [], [], [], []
    objs, irqs, pins = {}, {}, {}
    cur, cur_t = None, 0
    rstart, bstart, pend = {}, {}, {}
    t_first = None
    t_last = 0
    n_int = n_obj = 0

    def close_blocked(num, t):
        if num in bstart:
            a, why = bstart.pop(num)
            blocked[num].append([a, t, why])

    def irq_rec(irq):
        return irqs.setdefault(irq, {'irq': irq, 'name': 'IRQ%d' % irq, 'runs': [], 'yields': 0, 'open': None})

    for t, c, rest in evs:
        nf = NFIELDS[c]
        q = rest.split(None, nf) if nf else ([rest] if rest else [])
        try:
            nums = [(-1 if x == '-' else int(x)) for x in q[:nf]]
        except ValueError:
            continue
        if len(nums) < nf:
            continue
        name = q[nf] if len(q) > nf else ''
        t_last = max(t_last, t)
        if c == 'K':
            marks.append([t, name])
        elif c == 'C':
            num = nums[0]
            tasks[num] = {'num': num, 'name': name or '?', 'prio': nums[1], 'created': t, 'deleted': None,
                          'prio_changes': [], 'notify': [], 'fn': None, 'stack_words': None, 'stack_unit': None}
            order.append(num)
            run.setdefault(num, [])
            ready.setdefault(num, [])
            blocked.setdefault(num, [])
        elif c == 'k':
            if nums[0] in tasks:
                tasks[nums[0]].update({'fn': name or None, 'stack_words': nums[1], 'stack_unit': nums[2]})
        elif c == 'T':
            ticks.append(t)
        elif c == 'V':
            pass
        elif c in 'MF':
            heap.append({'t': t, 'op': c, 'addr': nums[0], 'size': nums[1],
                         'free': nums[2], 'task': nums[3]})
        elif c == 'Q':
            objs[nums[0]] = {'id': nums[0], 'type': OBJ_TYPES.get(nums[1], 'queue'), 'len': nums[2],
                             'level': [[t, nums[3]]], 'ops': [], 'line': 0, 'name': '', 'holder': [],
                             'sends': 0, 'recvs': 0, 'send_fail': 0, 'recv_fail': 0, 'max': nums[3]}
        elif c == 'Y':
            objs[nums[0]] = {'id': nums[0], 'type': 'timer', 'len': 0, 'period': nums[1], 'auto': nums[2],
                             'timer_name': name, 'expired': [], 'ops': [], 'line': 0, 'name': ''}
        elif c == 'N':
            ob = objs.get(nums[0])
            if ob is not None:
                if nums[1] > 0:
                    ob['line'] = nums[1]
                if name:
                    ob['name'] = name
        elif c in 'SsGgc':
            ob = objs.get(nums[0])
            if ob is None:
                continue
            lvl, ctx = nums[1], nums[2]
            if c in 'SGc':
                ob['level'].append([t, lvl])
                ob['max'] = max(ob['max'], lvl)
            key = {'S': 'sends', 's': 'send_fail', 'G': 'recvs', 'g': 'recv_fail'}.get(c)
            if key:
                ob[key] += 1
            if c != 'c' and n_obj < MAX_OBJ_EVENTS:
                ob['ops'].append([t, c, ctx])
                n_obj += 1
            if ob['type'] in ('mutex', 'recursive') and ctx < 100000:
                if c == 'G':
                    ob['holder'].append([t, None, ctx])
                elif c == 'S' and ob['holder'] and ob['holder'][-1][1] is None:
                    ob['holder'][-1][1] = t
        elif c in 'ex':
            ob = objs.get(nums[0])
            if ob is not None and n_obj < MAX_OBJ_EVENTS:
                ob['ops'].append([t, c, nums[2], nums[1]])
                ob['sends' if c == 'e' else 'recvs'] += 1
                n_obj += 1
        elif c == 'X':
            ob = objs.get(nums[0])
            if ob is not None and n_obj < MAX_OBJ_EVENTS:
                ob['expired'].append(t)
                n_obj += 1
        elif c == 'J':
            irq_rec(nums[0])['name'] = name or 'IRQ%d' % nums[0]
        elif c in 'ijy':
            ir = irq_rec(nums[0])
            if c == 'i':
                ir['open'] = t
            elif c == 'j' and ir['open'] is not None:
                if len(ir['runs']) < MAX_OBJ_EVENTS:
                    ir['runs'].append([ir['open'], t])
                ir['open'] = None
            elif c == 'y':
                ir['yields'] += 1
        elif c in 'lL':
            pn = pins.setdefault(nums[0], {'pin': nums[0], 'name': 'P%d' % nums[0], 'changes': []})
            if c == 'l':
                pn['name'] = name or pn['name']
                if not pn['changes']:
                    pn['changes'].append([t, nums[1]])
            elif len(pn['changes']) < MAX_OBJ_EVENTS:
                pn['changes'].append([t, nums[1]])
        elif c == 'Z':
            misuse.append([t, nums[0], name])
        else:
            if not nums:
                continue
            num = nums[0]
            if num not in tasks:
                continue
            if c == 'I':
                if t_first is None:
                    t_first = t
                if cur is not None and cur != num:
                    run[cur].append([cur_t, t])
                    n_int += 1
                if num in rstart:
                    ready[num].append([rstart.pop(num), t])
                    n_int += 1
                close_blocked(num, t)
                pend.pop(num, None)
                cur, cur_t = num, t
            elif c == 'O':
                if cur == num:
                    run[num].append([cur_t, t])
                    n_int += 1
                    cur = None
                if nums[1] == 1:
                    rstart.setdefault(num, t)
                else:
                    if num in rstart:     # ready -> blocked without running
                        ready[num].append([rstart.pop(num), t])
                    bstart[num] = (t, pend.pop(num, None))
            elif c == 'R':
                close_blocked(num, t)
                if num != cur and num not in rstart:
                    rstart[num] = t
            elif c == 'B':
                why = (name or '?', nums[1], nums[2])
                old = pend.get(num)
                if not (old and why[0] in 'du' and old[0] not in 'du'):   # keep the object, not the timeout
                    pend[num] = why
            elif c == 'U':
                pend[num] = ('U', 0, 0)
                if num in rstart:
                    ready[num].append([rstart.pop(num), t])
                    bstart[num] = (t, pend.pop(num))
            elif c == 'A':
                if len(tasks[num]['notify']) < MAX_OBJ_EVENTS:
                    tasks[num]['notify'].append([t, nums[1]])
            elif c == 'D':
                tasks[num]['deleted'] = t
                if num in rstart:
                    ready[num].append([rstart.pop(num), t])
                close_blocked(num, t)
            elif c in 'PHh':
                tasks[num]['prio_changes'].append([t, nums[1], {'P': 'set', 'H': 'inherit', 'h': 'disinherit'}[c]])
        if n_int > MAX_INTERVALS:
            break
    if cur is not None:
        run[cur].append([cur_t, t_last])
    for num, st in rstart.items():
        ready[num].append([st, t_last])
    for num in list(bstart):
        close_blocked(num, t_last)
    base = 0 if virtual else (t_first if t_first is not None else 0)
    total = max(t_last - base, 1)

    # ---- names
    tname = {n: tk['name'] for n, tk in tasks.items()}
    irq_name = {i: ir['name'] for i, ir in irqs.items()}

    def who(ctx):
        if ctx >= 100000:
            return 'ISR ' + irq_name.get(ctx - 100000, 'IRQ%d' % (ctx - 100000))
        return tname.get(ctx, 'main()' if ctx == 0 else '#%d' % ctx)

    used = {}
    for ob in sorted(objs.values(), key=lambda o: o['id']):
        nm = ob['name']
        ob['user'] = ob['line'] > 0 or (bool(nm) and nm != 'TmrQ')
        if not nm and 0 < ob['line'] <= len(src_lines):
            nm = var_on_line(src_lines[ob['line'] - 1]) or ''
        if not nm and ob['type'] == 'timer':
            nm = ob.get('timer_name') or ''
        nm = nm or '%s#%d' % (OBJ_LABEL[ob['type']], ob['id'])
        used[nm] = used.get(nm, 0) + 1
        ob['name'] = nm if used[nm] == 1 else '%s#%d' % (nm, used[nm])

    def reason(why, task):
        if why is None:
            return 'Blocked'
        k, obj, arg = why
        if task == 'Tmr Svc' and k in 'ru':
            return 'chờ lệnh/timer hết hạn'
        ob = objs.get(obj)
        if k == 'U':
            return 'Suspended'
        if k == 'd':
            return 'vTaskDelay(%d)' % arg
        if k == 'n':
            return 'chờ task notification'
        if k == 'e':
            return 'chờ bit 0x%x của %s' % (arg, ob['name'] if ob else 'event group')
        if k == 'u' and obj == 0:
            return 'vTaskDelayUntil'
        if ob is None or not ob['user']:       # the timer service task waiting on its command queue
            return 'chờ lệnh/timer hết hạn'
        if k == 's':
            return 'chờ gửi vào %s (đầy)' % ob['name']
        if ob['type'] in ('mutex', 'recursive', 'binary', 'counting'):
            return 'chờ lấy %s' % ob['name']
        return 'chờ nhận từ %s' % ob['name']

    def rel(iv):
        return [[x - base, y - base] for x, y in iv if y >= x]

    out_tasks = []
    for num in order:
        tk = tasks[num]
        r = rel(run[num])
        bl = [[a - base, b - base, reason(w, tk['name'])] for a, b, w in blocked[num] if b >= a]
        by = {}
        for a, b, w in bl:
            by[w] = by.get(w, 0) + (b - a)
        out_tasks.append({
            'num': num, 'name': tk['name'], 'prio': tk['prio'],
            'created': tk['created'] - base,
            'deleted': None if tk['deleted'] is None else tk['deleted'] - base,
            'prio_changes': [[x - base, pr, k] for x, pr, k in tk['prio_changes']],
            'run': r, 'ready': rel(ready[num]), 'blocked': bl, 'blocked_by': by,
            'notify': [[x - base, who(cx)] for x, cx in tk['notify']],
            'cpu': sum(y - x for x, y in r), 'switch_ins': len(r),
            'fn': tk['fn'], 'stack_words': tk['stack_words'], 'stack_unit': tk['stack_unit'],
        })
    # A task deleted and created again with the same name (Example 4.9) stays on one row.
    lanes = []
    for tk in out_tasks:
        for ln in lanes:
            if ln['name'] == tk['name'] and ln['deleted'] is not None and ln['deleted'] <= tk['created']:
                for k in ('run', 'ready', 'blocked', 'prio_changes', 'notify'):
                    ln[k] += tk[k]
                for w, d in tk['blocked_by'].items():
                    ln['blocked_by'][w] = ln['blocked_by'].get(w, 0) + d
                ln['cpu'] += tk['cpu']
                ln['switch_ins'] += tk['switch_ins']
                ln['deleted'] = tk['deleted']
                ln['instances'] += 1
                break
        else:
            tk['instances'] = 1
            lanes.append(tk)
    for ln in lanes:
        ln['cpu_pct'] = round(100.0 * ln['cpu'] / total, 2)
    for h in heap:
        h['t'] -= base
        h['task_name'] = tasks[h['task']]['name'] if h['task'] in tasks else '(main)'

    out_objs = []
    for ob in sorted(objs.values(), key=lambda o: o['id']):
        if not ob['user']:
            continue
        o2 = {k: v for k, v in ob.items() if k not in ('ops', 'level', 'holder', 'expired')}
        o2['ops'] = [[x[0] - base, x[1], who(x[2])] + x[3:] for x in ob['ops']]
        if 'level' in ob:
            o2['level'] = [[x - base, v] for x, v in ob['level']]
        if 'holder' in ob:
            o2['holder'] = [[a - base, (b if b is not None else t_last) - base, who(cx)] for a, b, cx in ob['holder']]
        if 'expired' in ob:
            o2['expired'] = [x - base for x in ob['expired']]
        out_objs.append(o2)
    out_irqs = [{'irq': ir['irq'], 'name': ir['name'], 'count': len(ir['runs']), 'yields': ir['yields'],
                 'runs': [[a - base, b - base] for a, b in ir['runs']],
                 'busy': sum(b - a for a, b in ir['runs'])}
                for ir in sorted(irqs.values(), key=lambda i: i['irq'])]
    out_pins = [{'pin': pn['pin'], 'name': pn['name'], 'changes': [[x - base, v] for x, v in pn['changes']]}
                for pn in sorted(pins.values(), key=lambda p: p['pin'])]
    return {
        'mode': 'kernel',
        'unit': 'tick' if virtual else 'us',
        'scheduler_started': t_first is not None,
        'total': total,
        'tick': 1 if virtual else tick_us,
        'ticks': [x - base for x in ticks],
        'marks': [[x - base, m] for x, m in marks],
        'tasks': lanes,
        'objects': out_objs,
        'irqs': out_irqs,
        'gpio': out_pins,
        'misuse': [[x - base, irq_name.get(i, 'IRQ%d' % i), api] for x, i, api in misuse],
        'heap': heap,
        'truncated': n_int > MAX_INTERVALS,
    }


# ---------------------------------------------------------------- debug (record & replay)

MAX_DEBUG_STEPS = 40000


def exe_src(workdir):
    """The user source file compiled in workdir."""
    for n in os.listdir(workdir):
        if n.endswith(('.c', '.txt')) and n != 'trace.txt':
            return os.path.join(workdir, n)
    return 'main.c'


def line_map(exe, src):
    """Return address of each __sanitizer_cov_trace_pc call -> (function, [source lines of that block]).
    A block runs from its hook call to the next hook call or the end of the function."""
    dis = subprocess.run(['objdump', '-d', '--no-show-raw-insn', exe], capture_output=True, text=True).stdout
    sites, funcs, fn, prev_call = [], [], None, None
    for line in dis.splitlines():
        m = re.match(r'^([0-9a-f]+) <([^>]+)>:', line)
        if m:
            fn = m.group(2)
            funcs.append(int(m.group(1), 16))
            prev_call = None
            continue
        m = re.match(r'^\s*([0-9a-f]+):\s+(.*)', line)
        if not m:
            continue
        addr = int(m.group(1), 16)
        if prev_call is not None:
            sites.append((prev_call, addr, fn))
            prev_call = None
        if '<__sanitizer_cov_trace_pc>' in m.group(2) and 'call' in m.group(2):
            prev_call = addr
    rows = []
    out = subprocess.run(['objdump', '--dwarf=decodedline', exe], capture_output=True, text=True).stdout
    for line in out.splitlines():
        m = re.match(r'^(\S+)\s+(\d+)\s+(0x[0-9a-f]+)', line)
        if m and m.group(1) == src:
            rows.append((int(m.group(3), 16), int(m.group(2))))
    rows.sort()
    funcs.sort()
    addrs = [a for a, _ in rows]
    res = {}
    for i, (call, ret, f) in enumerate(sites):
        nxt = sites[i + 1][0] if i + 1 < len(sites) and sites[i + 1][2] == f else None
        fend = funcs[bisect.bisect_right(funcs, call)] if bisect.bisect_right(funcs, call) < len(funcs) else 1 << 62
        end = min(nxt or fend, fend)
        k = bisect.bisect_right(addrs, call) - 1
        lines = [rows[k][1]] if k >= 0 else []
        for a, ln in rows[k + 1:]:
            if a >= end:
                break
            if ln not in lines:
                lines.append(ln)
        res[ret] = (f, lines)
    return res


STATE_VI = {'Running': 'Running', 'Ready': 'Ready', 'Blocked': 'Blocked', 'Suspended': 'Suspended', 'Deleted': 'Đã xoá'}


def debug_steps(path, code, exe, src, out_bytes):
    """Every executed block of the user's code and every kernel event, in order,
    each with a description and the state change it causes (replayed by the IDE)."""
    try:
        lmap = line_map(exe, src)
        with open(path, errors='replace') as f:
            lines = f.read().splitlines()
    except OSError:
        return None
    src_lines = code.split('\n')
    tasks, objs, irqs, pins = {}, {}, {}, {}
    steps, pend, last_line = [], {}, {}
    cur = None
    tick = 0
    out_off = 0
    t_first = None
    truncated = False

    def who(ctx):
        if ctx >= 100000:
            return 'ISR ' + irqs.get(ctx - 100000, 'IRQ%d' % (ctx - 100000))
        return tasks[ctx]['name'] if ctx in tasks else 'main()'

    def oname(i):
        return objs[i]['name'] if i in objs else '#%d' % i

    def src_text(n):
        return src_lines[n - 1].strip()[:120] if 0 < n <= len(src_lines) else ''

    state = {}

    def add(t, k, x, c=None, **kw):
        if 's' in kw:
            state[kw['s'][0]] = kw['s'][1]
        st = {'t': t, 'k': k, 'x': x, 'tk': tick}
        if c is not None:
            st['c'] = c
        st.update(kw)
        steps.append(st)

    for raw in lines:
        p = raw.split(None, 2)
        if len(p) < 2 or not p[0].isdigit() or p[1] not in NFIELDS and p[1] not in 'EWou':
            continue
        t, c = int(p[0]), p[1]
        nf = {'E': 2, 'W': 2, 'o': 1, 'u': 2}.get(c, NFIELDS.get(c, 0))
        q = (p[2] if len(p) > 2 else '').split(None, nf) if nf else ([p[2]] if len(p) > 2 else [])
        try:
            nums = [(-1 if x == '-' else int(x)) for x in q[:nf]]
        except ValueError:
            continue
        if len(nums) < nf:
            continue
        name = q[nf] if len(q) > nf else ''
        if len(steps) >= MAX_DEBUG_STEPS:
            truncated = True
            break
        if c == 'T':
            tick = nums[0]
            continue
        if c == 'I' and t_first is None:
            t_first = t
        if c == 'E':
            blk = lmap.get(nums[1])
            if not blk or not blk[1]:
                continue
            last_line[nums[0]] = blk[1]
            add(t, 'line', 'dòng %d: %s' % (blk[1][0], src_text(blk[1][0])), who(nums[0]), L=blk[1], ctx=nums[0], fn=blk[0])
        elif c == 'u':
            if steps and steps[-1]['k'] == 'line' and steps[-1].get('ctx') == nums[0] and steps[-1]['L'][0] == nums[1]:
                steps.pop()                   # the block step and the call step are the same line
            last_line[nums[0]] = [nums[1]]
            add(t, 'call', 'dòng %d: %s' % (nums[1], src_text(nums[1])), who(nums[0]), L=[nums[1]], ctx=nums[0])
        elif c == 'W':
            add(t, 'loop', 'lặp lại %d khối lệnh (vòng lặp không gọi API kernel)' % nums[1], who(nums[0]),
                L=last_line.get(nums[0], []), ctx=nums[0])
        elif c == 'o':
            text = out_bytes[out_off:nums[0]].decode('utf-8', 'replace')
            out_off = nums[0]
            add(t, 'out', 'in ra: ' + text.strip()[:160], who(cur) if cur else 'main()', out=out_off)
        elif c == 'C':
            tasks[nums[0]] = {'name': name or '?', 'prio': nums[1]}
            add(t, 'task', 'xTaskCreate → task "%s" (ưu tiên %d) → Ready' % (name, nums[1]), who(cur) if cur else 'main()',
                s=[nums[0], 'Ready', ''], new=[nums[0], name, nums[1]])
        elif c == 'I':
            cur = nums[0]
            add(t, 'sched', '%s → Running' % who(nums[0]), 'scheduler', s=[nums[0], 'Running', ''])
        elif c == 'O':
            if nums[1] == 1:
                add(t, 'sched', '%s → Ready (nhường CPU: bị chiếm quyền, hết time slice hoặc taskYIELD)' % who(nums[0]),
                    'scheduler', s=[nums[0], 'Ready', ''])
            else:
                why = pend.pop(nums[0], 'Blocked')
                add(t, 'sched', '%s → Blocked: %s' % (who(nums[0]), why), 'scheduler', s=[nums[0], 'Blocked', why])
        elif c == 'R':
            if state.get(nums[0]) in ('Blocked', 'Suspended'):
                add(t, 'sched', '%s → Ready (được đánh thức)' % who(nums[0]), 'kernel', s=[nums[0], 'Ready', ''])
        elif c == 'B':
            k, ob, arg = name, nums[1], nums[2]
            if k == 'd':
                why = 'vTaskDelay(%d)' % arg
            elif k == 'u':
                why = ('vTaskDelayUntil (đến tick %d)' % arg if ob == 0 and tasks.get(nums[0], {}).get('name') != 'Tmr Svc'
                       else 'chờ lệnh/timer hết hạn')
            elif k == 'n':
                why = 'chờ task notification'
            elif k == 'e':
                why = 'chờ bit 0x%x của %s' % (arg, oname(ob))
            elif ob in objs and objs[ob].get('user'):
                o = objs[ob]
                why = ('chờ gửi vào %s (đầy)' if k == 's' else
                       'chờ lấy %s' if o['type'] in ('mutex', 'recursive', 'binary', 'counting') else 'chờ nhận từ %s') % o['name']
            else:
                why = 'chờ lệnh/timer hết hạn'
            old = pend.get(nums[0])
            if not (old and k in 'du' and not old.startswith('vTask')):
                pend[nums[0]] = why
        elif c == 'U':
            add(t, 'sched', '%s → Suspended' % who(nums[0]), who(nums[1]), s=[nums[0], 'Suspended', 'vTaskSuspend'])
        elif c == 'D':
            add(t, 'sched', '%s bị xoá (vTaskDelete)' % who(nums[0]), 'kernel', s=[nums[0], 'Deleted', ''])
        elif c in 'PHh':
            lbl = {'P': 'vTaskPrioritySet', 'H': 'kế thừa ưu tiên (mutex)', 'h': 'trả ưu tiên kế thừa'}[c]
            add(t, 'prio', '%s → ưu tiên %d (%s)' % (who(nums[0]), nums[1], lbl), 'kernel', pr=[nums[0], nums[1]])
        elif c == 'Q':
            objs[nums[0]] = {'name': '', 'type': OBJ_TYPES.get(nums[1], 'queue'), 'len': nums[2], 'user': False}
        elif c == 'Y':
            objs[nums[0]] = {'name': name, 'type': 'timer', 'len': 0, 'user': False, 'period': nums[1]}
        elif c == 'N':
            o = objs.get(nums[0])
            if o is None:
                continue
            nm = name or (var_on_line(src_lines[nums[1] - 1]) if 0 < nums[1] <= len(src_lines) else '') or o['name']
            if nm == 'TmrQ':
                continue
            o['name'] = nm or '%s#%d' % (OBJ_LABEL[o['type']], nums[0])
            o['user'] = True
            add(t, 'obj', 'tạo %s "%s"%s' % (OBJ_LABEL[o['type']], o['name'], ' (%d phần tử)' % o['len'] if o['type'] == 'queue' else ''),
                who(cur) if cur else 'main()', newobj=[nums[0], o['name'], o['type'], o['len']])
        elif c in 'SsGgc':
            o = objs.get(nums[0])
            if not o or not o['user']:
                continue
            ty, nm, lv = o['type'], o['name'], nums[1]
            mtx = ty in ('mutex', 'recursive')
            sem = ty in ('binary', 'counting')
            verb = {'S': 'trả' if mtx else 'give' if sem else 'gửi vào', 'G': 'lấy' if mtx else 'take' if sem else 'nhận từ',
                    's': 'gửi thất bại (đầy):', 'g': 'nhận thất bại (rỗng/hết thời gian chờ):', 'c': 'khởi tạo'}[c]
            x = '%s %s' % (verb, nm) + (' → %d/%d' % (lv, o['len']) if c in 'SGc' and not mtx else '')
            hold = who(nums[2]) if mtx and c == 'G' else ('' if mtx and c == 'S' else None)
            add(t, 'obj', x, who(nums[2]), o=[nums[0], lv if c in 'SGc' else None, hold])
        elif c in 'ex':
            if nums[0] in objs and objs[nums[0]]['user']:
                add(t, 'obj', '%s bit 0x%x của %s' % ('set' if c == 'e' else 'xoá', nums[1], oname(nums[0])), who(nums[2]))
        elif c == 'X':
            if nums[0] in objs:
                add(t, 'obj', 'timer %s hết hạn → callback chạy trong Tmr Svc' % oname(nums[0]), 'Tmr Svc')
        elif c == 'A':
            add(t, 'obj', '%s nhận task notification' % who(nums[0]), who(nums[1]))
        elif c == 'J':
            irqs[nums[0]] = name or 'IRQ%d' % nums[0]
        elif c == 'i':
            add(t, 'isr', 'vào ISR %s (CPU rời task đang chạy)' % irqs.get(nums[0], 'IRQ%d' % nums[0]), 'ISR ' + irqs.get(nums[0], ''), isr=nums[0])
        elif c == 'j':
            add(t, 'isr', 'thoát ISR %s' % irqs.get(nums[0], 'IRQ%d' % nums[0]), 'ISR ' + irqs.get(nums[0], ''), isr=-1)
        elif c == 'y':
            add(t, 'isr', 'portYIELD_FROM_ISR(pdTRUE): đòi chuyển ngữ cảnh khi ISR kết thúc', 'ISR ' + irqs.get(nums[0], ''))
        elif c == 'l':
            pins[nums[0]] = name or 'P%d' % nums[0]
        elif c == 'L':
            add(t, 'gpio', '%s = %d' % (pins.get(nums[0], 'P%d' % nums[0]), nums[1]), who(cur) if cur else 'main()', g=[nums[0], nums[1]])
        elif c in 'MF':
            add(t, 'heap', ('pvPortMalloc(%d B)' % nums[1] if c == 'M' else 'vPortFree(%d B)' % nums[1]) +
                (' → heap còn %d B' % nums[2] if nums[2] >= 0 else ''), who(nums[3]), hp=nums[2])
        elif c == 'Z':
            add(t, 'warn', 'SAI: %s gọi trong ISR' % name, 'ISR')
    base = t_first or 0
    for st in steps:
        st['t'] = round((st['t'] - base) / 1000.0, 3)
    return {'steps': steps, 'truncated': truncated, 'src': src,
            'tasks': {n: v['name'] for n, v in tasks.items()},
            'objs': {i: [o['name'], o['type'], o['len']] for i, o in objs.items() if o['user']},
            'pins': pins}


# ---------------------------------------------------------------- MCU resources

def read_callgraph(paths):
    """gcc -fcallgraph-info=su files -> {function: (frame bytes, qualifier) or None}, edges."""
    nodes, edges = {}, {}
    for p in paths:
        try:
            with open(p, errors='replace') as f:
                text = f.read()
        except OSError:
            continue
        text = re.sub(r'"[^"\s]*\.c:(\w+)"', r'"\1"', text)     # static functions: "path/file.c:name"
        for title, label in re.findall(r'node: \{ title: "([^"]+)" label: "([^"]*)"', text):
            m = re.search(r'(\d+) bytes \(([^)]*)\)', label)
            if m:
                nodes[title] = (int(m.group(1)), m.group(2))
            else:
                nodes.setdefault(title, None)        # declared only: external or builtin
        for s, d in re.findall(r'edge: \{ sourcename: "([^"]+)" targetname: "([^"]+)"', text):
            edges.setdefault(s, set()).add(d)
    return nodes, edges


def stack_depth(fn, nodes, edges, indirect=None):
    """Worst-case stack bytes from fn down its static call graph.
    indirect: functions a call through a pointer may reach."""
    memo, notes = {}, set()

    def go(f, path):
        if f in memo:
            return memo[f]
        if f in path:
            notes.add('đệ quy qua %s: không chặn được' % f)
            return 0
        if f == '__indirect_call':
            own, calls = 0, list(indirect or [])
            if not calls:
                notes.add('gọi qua con trỏ hàm: không tính')
        elif nodes.get(f) is None:
            if f in EXTERN_STACK:
                notes.add('%s ≈ %d B (giả định)' % (f, EXTERN_STACK[f]))
                return EXTERN_STACK[f]
            return 0                                   # simulator hook or tiny libc helper
        else:
            own, qual = nodes[f]
            calls = edges.get(f, ())
            if 'dynamic' in qual and 'bounded' not in qual:
                notes.add('%s dùng stack động (alloca/VLA)' % f)
        path.add(f)
        best = max([go(c, path) for c in calls if not SIM_ONLY.match(c) and c not in ISR_ONLY] or [0])
        path.discard(f)
        memo[f] = own + best
        return memo[f]

    return go(fn, set()), sorted(notes)


def _size_of(obj):
    """(text, data, bss) of an object file."""
    r = subprocess.run(['size', obj], capture_output=True, text=True)
    m = re.search(r'^\s*(\d+)\s+(\d+)\s+(\d+)', r.stdout, re.M)
    return (int(m.group(1)), int(m.group(2)), int(m.group(3))) if m else (0, 0, 0)


def mcu_report(an, o, tr, code):
    """Static stack estimate per task and RAM budget for the chosen MCU."""
    kdir, user_obj = an
    mcu = MCUS[o['mcu']]
    rep = {'mcu': o['mcu'], 'mcu_name': mcu['name'], 'ram': mcu['ram'], 'ctx': mcu['ctx'],
           'arch': 'i386, gcc -m32 -Os' if m32_flags() else 'x86-64, gcc -Os', 'tasks': [], 'notes': []}
    cis = [p for p in glob.glob(os.path.join(kdir, '*.ci'))
           if not os.path.basename(p).startswith(HOST_OBJS)]
    if user_obj:
        cis.append(os.path.splitext(user_obj)[0] + '.ci')
    nodes, edges = read_callgraph(cis)
    if not nodes:
        rep['notes'].append('Không phân tích được stack (gcc không hỗ trợ -fcallgraph-info).')
    callbacks = re.findall(r'xTimerCreate(?:Static)?\s*\((?:[^,]*,){4}\s*(?:\(\s*\w+\s*\)\s*)?(\w+)', code)
    entry = {'IDLE': 'prvIdleTask', 'Tmr Svc': 'prvTimerTask'}
    for tk in (tr or {}).get('tasks', []) if nodes else []:
        fn = tk.get('fn')
        fn = re.findall(r'\w+', fn)[-1] if fn and re.findall(r'\w+', fn) else entry.get(tk['name'])
        words, unit = tk.get('stack_words'), tk.get('stack_unit') or 4
        row = {'task': tk['name'], 'fn': fn, 'words': words, 'have': words * unit if words else None, 'notes': []}
        if fn and nodes.get(fn):
            d, notes = stack_depth(fn, nodes, edges, callbacks if fn == 'prvTimerTask' else None)
            row.update({'used': d + mcu['ctx'], 'frames': d, 'notes': notes})
            if row['have']:
                row['margin'] = row['have'] - row['used']
                row['status'] = ('overflow' if row['margin'] < 0 else
                                 'tight' if row['margin'] < 0.2 * row['have'] else 'ok')
        else:
            row['status'] = 'unknown'
        rep['tasks'].append(row)
    # RAM: .data + .bss of the kernel (the heap_1/2/4 array is in .bss) and of the program
    k_static = sum(sum(_size_of(p)[1:]) for p in glob.glob(os.path.join(kdir, '*.o'))
                   if not os.path.basename(p).startswith(HOST_OBJS))
    if o['heap'] in (1, 2, 4):
        k_static -= o['heap_size']             # the ucHeap array; counted below as what was really used
    u_static = sum(_size_of(user_obj)[1:]) if user_obj else 0
    heap = [h for h in (tr or {}).get('heap', []) if h['free'] >= 0]
    peak = o['heap_size'] - min(h['free'] for h in heap) if heap else sum(
        h['size'] for h in (tr or {}).get('heap', []) if h['op'] == 'M' and h['size'] > 0)
    parts = [['Kernel (.data+.bss)', k_static], ['Chương trình (.data+.bss)', u_static],
             ['Heap dùng thực tế (đỉnh), cần configTOTAL_HEAP_SIZE ≥ giá trị này', peak]]
    rep['heap_peak'] = peak
    if o['static_alloc']:
        parts.append(['Idle + Timer task tĩnh (stack + TCB)', 2 * (128 * 4 + 100)])
    if mcu['msp']:
        parts.append(['Main stack MSP (ISR, main())', mcu['msp']])
    rep['ram_parts'] = parts
    rep['ram_total'] = sum(v for _, v in parts)
    return rep


HEAP_API = {   # heap_N.c -> functions it does not provide
    1: ('vPortFree', 'xPortGetMinimumEverFreeHeapSize', 'vPortGetHeapStats', 'vPortDefineHeapRegions'),
    2: ('xPortGetMinimumEverFreeHeapSize', 'vPortGetHeapStats', 'vPortDefineHeapRegions'),
    3: ('xPortGetFreeHeapSize', 'xPortGetMinimumEverFreeHeapSize', 'vPortGetHeapStats', 'vPortDefineHeapRegions'),
    4: ('vPortDefineHeapRegions',),
    5: (),
}


def hints(res, o):
    """Short explanations for failures that are lessons of the book."""
    h = []
    log, err = res.get('log', ''), res.get('stderr', '')
    for fn in sorted(set(re.findall(r"undefined reference to `(\w+)'", log))):
        if fn in HEAP_API.get(o['heap'], ()):
            h.append('heap_%d không có %s(). Chọn heap khác (bảng so sánh heap_1..5, chương 3).' % (o['heap'], fn))
        elif fn == 'main':
            h.append('File không có main(): cần main() tạo task rồi gọi vTaskStartScheduler().')
    if 'heap_1.c' in err and 'configASSERT' in err:
        h.append('heap_1 không giải phóng bộ nhớ: không dùng được vTaskDelete()/vPortFree() với heap_1.')
    if 'heap_5.c' in err and 'configASSERT' in err:
        h.append('heap_5: phải gọi vPortDefineHeapRegions() trong main() trước mọi lần cấp phát (trước xTaskCreate).')
    if 'heap exhausted' in err or 'MallocFailedHook' in res.get('stdout', ''):
        h.append('Hết heap: tăng configTOTAL_HEAP_SIZE hoặc giảm stack depth của task.')
    if 'timers.c' in err and 'configASSERT' in err:
        h.append('Không tạo được Timer task (Tmr Svc: TCB + 256 word stack + queue lệnh ≈ 1,4 KB heap) khi '
                 'vTaskStartScheduler(): tăng configTOTAL_HEAP_SIZE.')
    if res.get('returncode') == 4:
        h.append('ISR không được block: trong ISR chỉ dùng API ...FromISR() (không có tham số thời gian chờ), '
                 'rồi kết thúc bằng portYIELD_FROM_ISR(xHigherPriorityTaskWoken) (chương 7).')
    tr = res.get('trace')
    if tr and tr.get('misuse'):
        apis = sorted(set(m[2] for m in tr['misuse']))
        h.append('Gọi API của task trong ISR: %s. Trên phần cứng có thể làm hỏng kernel; dùng bản ...FromISR().' % ', '.join(apis))
    if res.get('ok') and res.get('returncode') is None and tr is not None and not tr['scheduler_started']:
        h.append('Scheduler không chạy task nào: vTaskStartScheduler() trả về vì không đủ heap để tạo Idle/Timer task.')
    rep = res.get('mcu')
    if rep:
        for t in rep['tasks']:
            if t.get('status') == 'overflow':
                fix = {'IDLE': 'configMINIMAL_STACK_SIZE (Idle hook chạy trên stack này)',
                       'Tmr Svc': 'configTIMER_TASK_STACK_DEPTH (callback của timer chạy trên stack này)'}.get(
                    t['task'], 'tham số stack depth của xTaskCreate()')
                h.append('Task "%s" có thể TRÀN STACK: cần ≈ %d B, được cấp %d B (%d word). Tăng %s.' % (
                    t['task'], t['used'], t['have'], t['words'], fix))
        if rep['ram'] and rep['ram_total'] > rep['ram']:
            h.append('Chương trình cần ≈ %d B RAM, nhiều hơn RAM của %s (%d B): trên chip thật sẽ không chạy được; '
                     'giảm stack depth của task hoặc dữ liệu tĩnh.' % (rep['ram_total'], rep['mcu'].upper(), rep['ram']))
    return h


MODES = ('freertos', 'c')


def detect_mode(code, filename=''):
    """freertos: uses the FreeRTOS API; c: any other C program."""
    return 'freertos' if re.search(r'\b(vTaskStartScheduler|xTaskCreate(Static)?)\b', code) else 'c'


def simulate(code, filename, opts, override=None):
    """opts: from the IDE; '// frsim:' lines in the file replace them;
    override (explicit command-line flags) replaces both."""
    opts = dict(opts or {})
    file_opts = directives(code)
    opts.update(file_opts)
    opts.update(override or {})
    o = normalize(opts)
    mode = opts.get('mode', 'auto')
    if mode not in MODES:
        mode = detect_mode(code, filename)
    o['mode'] = mode
    base = {'options': o, 'mode': mode, 'directives': file_opts}
    with _run_lock:          # one simulation at a time: keeps timing undisturbed
        workdir = tempfile.mkdtemp(prefix='frsim-')
        try:
            an = None
            try:
                if mode == 'freertos':
                    exe, log, an = compile_user(code, filename, o, workdir)
                else:
                    exe, log = compile_c(code, filename, workdir)
            except BuildError as e:
                return dict(base, ok=False, phase='build', log=str(e))
            if exe is None:
                res = dict(base, ok=False, phase='compile', log=log)
                res['hints'] = hints(res, o) if mode == 'freertos' else []
                return res
            res = run_exe(exe, o, workdir, mode, code)
            res.update(base, ok=True, phase='run', log=log)
            if an:
                res['mcu'] = mcu_report(an, o, res['trace'], code)
            res['hints'] = hints(res, o) if mode == 'freertos' else []
            return res
        finally:
            shutil.rmtree(workdir, ignore_errors=True)


# ---------------------------------------------------------------- CLI output

def fmt_t(tr, t):
    return '%g ms' % round(t / 1000, 3) if tr['unit'] == 'us' else '%g' % t


def gantt(tr, end, cols=100):
    """ASCII timeline over [0, end]: '#' running, '.' ready, ' ' blocked;
    '!' ISR, digits queue/semaphore level, '-' '_' GPIO high/low."""
    if not tr or not tr['tasks']:
        return ''
    step = end / cols
    rows = []

    def cell(t):
        return min(max(int(t / step), 0), cols - 1)

    def paint(row, iv, ch, over):
        for x in iv:
            a, b = x[0], x[1]
            if b <= 0 or a >= end:
                continue
            for i in range(cell(a), cell(max(b - 1e-9, a)) + 1):
                if over or row[i] == ' ':
                    row[i] = ch

    def level_at(series, t):
        v = None
        for x, lv in series:
            if x > t:
                break
            v = lv
        return v

    for tk in sorted(tr['tasks'], key=lambda t: -t['prio']):
        row = [' '] * cols
        paint(row, tk.get('ready', []), '.', False)
        paint(row, tk['run'], '#', True)
        rows.append(('%s(p%s)' % (tk['name'], tk['prio']), row))
    for ir in tr.get('irqs', []):
        row = [' '] * cols
        for a, _ in ir['runs']:
            if 0 <= a < end:
                row[cell(a)] = '!'
        rows.append(('ISR ' + ir['name'], row))
    for ob in tr.get('objects', []):
        if 'level' not in ob or ob['type'] == 'eventgroup':
            continue
        lv = sorted(ob['level'])
        row = [' '] * cols
        for i in range(cols):
            v = level_at(lv, (i + 1) * step)
            row[i] = ' ' if v is None else str(min(v, 9))
        rows.append((ob['name'], row))
    for pn in tr.get('gpio', []):
        ch = sorted(pn['changes'])
        row = [' '] * cols
        for i in range(cols):
            v = level_at(ch, (i + 1) * step)
            row[i] = ' ' if v is None else ('-' if v else '_')
        rows.append((pn['name'], row))
    w = max(len(n) for n, _ in rows)
    lines = ['%-*s |%s|' % (w, n, ''.join(r)) for n, r in rows]
    if tr.get('ticks'):
        axis = [' '] * cols
        for t in tr['ticks']:
            if 0 <= t <= end:
                axis[cell(t)] = '|'
        lines.append('%-*s |%s|' % (w, 'tick', ''.join(axis)))
    lines.append('%-*s  0%s%s' % (w, '', ' ' * (cols - 8), fmt_t(tr, end)))
    return '\n'.join(lines)


def print_report(res, gantt_arg):
    if res.get('directives'):
        print('[sim] options from the file: ' + ' '.join('%s=%s' % kv for kv in res['directives'].items()))
    if res.get('log', '').strip():
        print('---- build ----')
        print(res['log'].rstrip())
    for x in res.get('hints', []):
        print('[hint] ' + x)
    if not res['ok']:
        print('---- %s FAILED ----' % res.get('phase', '?'))
        return 1
    if res.get('stdout'):
        print('---- output ----')
        sys.stdout.write(res['stdout'])
    if res.get('stderr'):
        print('---- stderr ----')
        sys.stdout.write(res['stderr'])
    print('---- end: %s ----' % res['exit'])
    tr = res['trace']
    if not tr or not tr['scheduler_started']:
        if res['mode'] == 'freertos':
            print('[sim] the scheduler never ran a task (vTaskStartScheduler() returned: heap too small? see chapter 3)')
        return 0
    print('\n%-16s %4s %12s %7s %9s  %s' % ('task', 'prio', 'cpu', 'cpu %', 'switch-in', 'blocked most on'))
    for t in sorted(tr['tasks'], key=lambda t: -t['prio']):
        by = sorted(t.get('blocked_by', {}).items(), key=lambda kv: -kv[1])[:2]
        print('%-16s %4d %12s %7.2f %9d  %s' % (t['name'], t['prio'], fmt_t(tr, t['cpu']), t['cpu_pct'], t['switch_ins'],
                                                '; '.join('%s %s' % (w, fmt_t(tr, d)) for w, d in by)))
    if tr.get('objects'):
        print('\n%-16s %-12s %5s %6s %6s %6s %6s %5s' % ('object', 'type', 'len', 'send', 'recv', 'full', 'empty', 'max'))
        for ob in tr['objects']:
            if ob['type'] == 'timer':
                print('%-16s %-12s period %d ticks, expired %d times' % (
                    ob['name'], 'auto-reload' if ob['auto'] else 'one-shot', ob['period'], len(ob['expired'])))
            elif ob['type'] == 'eventgroup':
                print('%-16s %-12s set %d, clear %d' % (ob['name'], 'event group', ob['sends'], ob['recvs']))
            else:
                print('%-16s %-12s %5s %6d %6d %6d %6d %5s' % (ob['name'], ob['type'], ob['len'], ob['sends'], ob['recvs'],
                                                              ob['send_fail'], ob['recv_fail'], ob['max']))
    for ir in tr.get('irqs', []):
        print('ISR %-14s IRQ %-3d %5d runs, %d yield requests' % (ir['name'], ir['irq'], ir['count'], ir['yields']))
    if tr.get('heap'):
        last = [h for h in tr['heap'] if h['free'] >= 0]
        print('\nheap_%d: %d allocations, %d frees' % (
            tr['heap_scheme'], sum(h['op'] == 'M' for h in tr['heap']),
            sum(h['op'] == 'F' for h in tr['heap'])), end='')
        if last:
            print(', free now %d / %d bytes, minimum %d' % (
                last[-1]['free'], tr['heap_size'], min(h['free'] for h in last)), end='')
        print()
    rep = res.get('mcu')
    if rep:
        print('\nMCU: %s' % rep['mcu_name'])
        print('RAM ≈ %d B%s: %s' % (rep['ram_total'], ' of %d B' % rep['ram'] if rep['ram'] else '',
                                    ', '.join('%s %d' % (n, v) for n, v in rep['ram_parts'])))
        print('stack, static estimate (%s) + %d B context frame:' % (rep['arch'], rep['ctx']))
        for t in rep['tasks']:
            if t.get('used') is None:
                print('  %-16s %-22s  not analysed' % (t['task'], t['fn'] or '?'))
            else:
                print('  %-16s %-22s need %5d B, have %5s B  %s' % (
                    t['task'], t['fn'], t['used'], t['have'] or '?',
                    {'ok': 'OK', 'tight': 'TIGHT', 'overflow': 'OVERFLOW'}.get(t.get('status'), '')))
    if res.get('debug'):
        d = res['debug']
        print('\ndebug: %d steps%s (first 400)' % (len(d['steps']), ', truncated' if d['truncated'] else ''))
        for st in d['steps'][:400]:
            print('%10.3f ms  tick %-6s %-12s %s' % (st['t'], st['tk'], (st.get('c') or '')[:12], st['x']))
    g = 20.0 if gantt_arg is None else gantt_arg
    if g:
        end = g * 1000 if tr['unit'] == 'us' else g
        if g < 0:
            end = tr['total']
        print('\ntimeline (# running, . ready, blank blocked, ! ISR, digit = queue/semaphore level, -_ GPIO):')
        print(gantt(tr, min(end, tr['total'])))
    return 0


# ---------------------------------------------------------------- web server

def example_key(name):
    """ex4_10 after ex4_9: compare the numbers in the name as numbers."""
    return [int(x) if x.isdigit() else x for x in re.split(r'(\d+)', name)]


def example_title(path):
    """First line of the leading comment, without the '// frsim:' options."""
    try:
        with open(path, encoding='utf-8', errors='replace') as f:
            head = f.read(400)
    except OSError:
        return ''
    m = re.match(r'\s*/\*(.*?)(\*/|$)', head, re.S) or re.match(r'\s*//\s*(.*)', head)
    lines = [re.sub(r'^\s*\*?\s*', '', ln) for ln in (m.group(1) if m else '').split('\n')]
    t = next((ln for ln in lines if ln.strip()), '')
    t = re.split(r'\s+frsim:|\*/', t)[0].strip().rstrip('.,:')
    return t[:90]


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        pass

    def _send(self, code, body, ctype='application/json'):
        if isinstance(body, (dict, list)):
            body = json.dumps(body).encode()
        elif isinstance(body, str):
            body = body.encode()
        self.send_response(code)
        self.send_header('Content-Type', ctype + '; charset=utf-8')
        self.send_header('Content-Length', str(len(body)))
        self.send_header('Cache-Control', 'no-store')
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        u = urllib.parse.urlparse(self.path)
        if u.path in ('/', '/index.html'):
            with open(os.path.join(WEB, 'index.html'), 'rb') as f:
                return self._send(200, f.read(), 'text/html')
        if u.path == '/api/examples':
            out = []
            for d in sorted(os.listdir(EXAMPLES)) if os.path.isdir(EXAMPLES) else []:
                if os.path.isdir(os.path.join(EXAMPLES, d)):
                    for n in sorted(os.listdir(os.path.join(EXAMPLES, d)), key=example_key):
                        out.append({'mode': d, 'path': d + '/' + n, 'title': example_title(os.path.join(EXAMPLES, d, n))})
            return self._send(200, out)
        if u.path.startswith('/api/examples/'):
            rel = os.path.normpath(urllib.parse.unquote(u.path[len('/api/examples/'):]))
            p = os.path.join(EXAMPLES, rel)
            if not rel.startswith('..') and os.path.isfile(p):
                with open(p, encoding='utf-8', errors='replace') as f:
                    return self._send(200, f.read(), 'text/plain')
        if u.path == '/api/defaults':
            return self._send(200, dict(normalize({}), mcus=MCUS))
        self._send(404, {'error': 'not found'})

    def do_POST(self):
        if self.path != '/api/run':
            return self._send(404, {'error': 'not found'})
        n = int(self.headers.get('Content-Length', 0))
        if n > (4 << 20):
            return self._send(413, {'error': 'file too large'})
        try:
            req = json.loads(self.rfile.read(n))
            res = simulate(req.get('code', ''), req.get('filename', 'main.c'), req.get('options', {}))
        except Exception as e:          # report, never kill the server
            res = {'ok': False, 'phase': 'server', 'log': repr(e)}
        self._send(200, res)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    r = sub.add_parser('run', help='compile and run a file')
    r.add_argument('file')
    for name, (_, typ, default, allowed) in OPTIONS.items():
        r.add_argument('--' + name.replace('_', '-'), type=typ, default=None,
                       choices=allowed if typ is str else None, help='default %s' % default)
    r.add_argument('--mode', choices=('auto',) + MODES, default='auto',
                   help='freertos | c (own C program); auto = detect')
    r.add_argument('--gantt', type=float, default=None,
                   help='ASCII timeline length: ms (real time) or ticks (st_time); 0 = off, -1 = all')
    r.add_argument('--json', action='store_true', help='print the raw JSON result')
    s = sub.add_parser('serve', help='start the web IDE')
    s.add_argument('--port', type=int, default=8765)
    a = ap.parse_args()

    if not os.path.isfile(os.path.join(KERNEL, 'tasks.c')):
        sys.exit('FreeRTOS kernel not found at %s (set FREERTOS_KERNEL)' % KERNEL)

    if a.cmd == 'run':
        with open(a.file, encoding='utf-8', errors='replace') as f:
            code = f.read()
        flags = {k: getattr(a, k) for k in OPTIONS if getattr(a, k) is not None}
        res = simulate(code, a.file, {'mode': a.mode}, flags)
        if a.json:
            print(json.dumps(res, indent=1))
            return 0 if res['ok'] else 1
        return print_report(res, a.gantt)

    srv = http.server.ThreadingHTTPServer(('127.0.0.1', a.port), Handler)
    print('FreeRTOS simulator IDE: http://127.0.0.1:%d  (Ctrl+C to stop)' % a.port)
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == '__main__':
    sys.exit(main())

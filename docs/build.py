"""Build book.json for the Mastering FreeRTOS walkthrough page from frsim runs."""
import bisect
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
RUNS = os.path.join(HERE, '..', 'runs')
SRC = os.path.expanduser('~/workspace/self-learning/freeRTOS/simulator/examples/book')
sys.path.insert(0, HERE)
from narr import CHAPTERS, N  # noqa: E402

MAX_IV = 3000          # intervals per lane kept for the overview
MAX_STEPS = 22


def tick_fn(ticks):
    """Real time (us) -> tick number, linear between recorded tick interrupts."""
    ts = [0] + list(ticks)

    def f(t):
        if t <= 0:
            return round(t / (ts[1] if len(ts) > 1 else 1000), 4)
        i = bisect.bisect_right(ts, t) - 1
        if i >= len(ts) - 1:
            per = (ts[-1] - ts[-2]) if len(ts) > 2 else 1000
            return round(i + (t - ts[-1]) / per, 4)
        return round(i + (t - ts[i]) / (ts[i + 1] - ts[i]), 4)
    return f


def clip(iv, end):
    out = [x for x in iv if x[0] <= end]
    return out[:MAX_IV]


def level_at(series, t):
    v = None
    for x, lv in series:
        if x > t + 1e-9:
            break
        v = lv
    return v


def build(name):
    r = json.load(open(os.path.join(RUNS, name + '.json')))
    tr = r['trace']
    nr = N[name]
    T = tick_fn(tr['ticks'])
    total = T(tr['total'])
    # ---- window
    if nr['win'] == 'irq':
        irq = tr['irqs'][0]['runs'][0]
        t0 = T(irq[0])
        idle = next(t for t in tr['tasks'] if t['name'] == 'IDLE')
        back = min([T(a) for a, b in idle['run'] if T(a) > t0] or [t0 + 1])
        win = [round(t0 - 0.15, 3), round(back + 0.15, 3)]
    else:
        win = list(nr['win'])
    view_end = min(total, max(win[1] * 6, win[1] + 30)) if nr['win'] != 'irq' else min(total, win[1] + 520)
    # ---- lanes
    lanes = []
    keep_tmr = any(o['type'] == 'timer' for o in tr['objects']) or name in ('ex7_3', 'ex9_1')
    for t in sorted(tr['tasks'], key=lambda t: (-t['prio'], t['num'])):
        if t['name'] == 'Tmr Svc' and not keep_tmr:
            continue
        lanes.append({
            'kind': 'task', 'name': t['name'], 'prio': t['prio'],
            'run': clip([[T(a), T(b)] for a, b in t['run']], view_end),
            'ready': clip([[T(a), T(b)] for a, b in t['ready']], view_end),
            'blocked': clip([[T(a), T(b), w] for a, b, w in t['blocked']], view_end),
            'prio': t['prio'],
            'pc': [[T(a), p, k] for a, p, k in t['prio_changes'] if T(a) <= view_end][:MAX_IV],
            'notify': [[T(a), w] for a, w in t['notify'] if T(a) <= view_end][:MAX_IV],
            'deleted': T(t['deleted']) if t['deleted'] is not None else None,
        })
    for i in tr['irqs']:
        lanes.append({'kind': 'isr', 'name': 'ISR ' + i['name'], 'run': clip([[T(a), T(b), round(b - a)] for a, b in i['runs']], view_end)})
    for o in tr['objects']:
        ln = {'kind': 'obj', 'type': o['type'], 'name': o['name'], 'len': o.get('len', 0)}
        if o['type'] == 'timer':
            ln.update(period=o['period'], auto=o['auto'], expired=[T(x) for x in o['expired'] if T(x) <= view_end])
        elif o['type'] == 'eventgroup':
            ln['ops'] = [[T(x[0]), x[1], x[2], x[3]] for x in o['ops'] if T(x[0]) <= view_end][:MAX_IV]
        else:
            ln['level'] = [[T(a), v] for a, v in o['level'] if T(a) <= view_end][:MAX_IV]
            ln['max'] = o['max']
            if o['type'] in ('mutex', 'recursive'):
                ln['holder'] = [[T(a), T(b), w] for a, b, w in o['holder'] if T(a) <= view_end][:MAX_IV]
        lanes.append(ln)
    for g in tr['gpio']:
        lanes.append({'kind': 'gpio', 'name': g['name'], 'changes': [[T(a), v] for a, v in g['changes'] if T(a) <= view_end]})
    # ---- steps inside the window
    a0, a1 = win
    inw = lambda t: a0 - 1e-6 <= t <= a1
    ev = []
    running = {}
    for t in tr['tasks']:
        for a, b in t['run']:
            running.setdefault(t['name'], []).append((T(a), T(b)))
    def was_running(task, t):
        return any(abs(b - t) < 0.002 for a, b in running.get(task, []))
    for t in tr['tasks']:
        n = t['name']
        if n == 'Tmr Svc' and not keep_tmr:
            continue
        for a, b in t['run']:
            if inw(T(a)):
                ev.append((T(a), 2, '<b>%s</b> → Running' % n, 'run'))
        for a, b in t['ready']:
            if inw(T(a)) and n != 'IDLE':
                why = 'bị chiếm quyền hoặc hết time slice' if was_running(n, T(a)) else 'được đánh thức'
                ev.append((T(a), 1, '%s → Ready (%s)' % (n, why), 'ready'))
        for a, b, w in t['blocked']:
            if inw(T(a)):
                ev.append((T(a), 1, '%s → Blocked: %s' % (n, w), 'block'))
        for a, p, k in t['prio_changes']:
            if inw(T(a)):
                ev.append((T(a), 0, '%s đổi ưu tiên → p%d%s' % (n, p, {'inherit': ' (kế thừa từ mutex)', 'disinherit': ' (trả ưu tiên kế thừa)'}.get(k, '')), 'prio'))
        for a, w in t['notify']:
            if inw(T(a)):
                ev.append((T(a), 0, '%s nhận notification từ %s' % (n, w), 'obj'))
        if t['deleted'] is not None and inw(T(t['deleted'])):
            ev.append((T(t['deleted']), 0, '%s bị xoá (vTaskDelete)' % n, 'block'))
    for i in tr['irqs']:
        for a, b in i['runs']:
            if inw(T(a)):
                ev.append((T(a), 0, '<b>ISR %s</b> chạy (%d µs)' % (i['name'], b - a), 'isr'))
    for o in tr['objects']:
        if o['type'] == 'timer':
            for x in o['expired']:
                if inw(T(x)):
                    ev.append((T(x), 0, 'Timer %s hết hạn → callback chạy trong Tmr Svc' % o['name'], 'obj'))
            continue
        for x in o['ops']:
            t, op, who = T(x[0]), x[1], x[2]
            if not inw(t):
                continue
            if o['type'] == 'eventgroup':
                ev.append((t, 0, '%s %s bit 0x%x của %s' % (who, 'set' if op == 'e' else 'xoá', x[3], o['name']), 'obj'))
                continue
            lv = level_at(o['level'], x[0])
            sem = o['type'] in ('binary', 'counting')
            mtx = o['type'] in ('mutex', 'recursive')
            if op == 'S':
                txt = '%s %s %s' % (who, 'trả' if mtx else 'give' if sem else 'gửi vào', o['name'])
            elif op == 'G':
                txt = '%s %s %s' % (who, 'lấy' if mtx else 'take' if sem else 'nhận từ', o['name'])
            elif op == 's':
                txt = '%s gửi vào %s thất bại (đầy)' % (who, o['name'])
            else:
                txt = '%s nhận từ %s thất bại (rỗng, hết thời gian chờ)' % (who, o['name'])
            if lv is not None and op in 'SG' and not mtx:
                txt += ' → %d/%d' % (lv, o['len'])
            ev.append((t, 0, txt, 'obj'))
    for g in tr['gpio']:
        prev = None
        for a, v in g['changes']:
            if inw(T(a)) and prev is not None and v != prev:
                ev.append((T(a), 0, '%s %s' % (g['name'], '= 0 (nhấn)' if v == 0 else '= 1 (nhả)'), 'gpio'))
            prev = v
    kinds = nr.get('kinds')
    long_win = a1 - a0 > 100
    ev = [e for e in ev if (not kinds or e[3] in kinds) and not (long_win and e[3] == 'run' and 'IDLE' in e[2])]
    ev.sort(key=lambda e: (e[0], e[1]))
    steps = [[round(t, 3), txt, k] for t, _, txt, k in ev]
    more = max(0, len(steps) - MAX_STEPS)
    steps = steps[:MAX_STEPS]
    # ---- measured
    meas = {
        'tasks': [[t['name'], t['prio'], t['cpu_pct'], t['switch_ins']] for t in sorted(tr['tasks'], key=lambda t: (-t['prio'], t['num']))],
        'objects': [[o['name'], o['type'], o.get('sends', 0), o.get('recvs', 0), o.get('max', ''), len(o.get('expired', []))] for o in tr['objects']],
        'irqs': [[i['name'], i['count'], i['yields']] for i in tr['irqs']],
        'total': round(total), 'tick_ms': round(tr['total'] / 1000 / max(total, 1), 3),
    }
    out = [l for l in r['stdout'].replace('\r', '').split('\n') if l.strip()]
    ch = int(name[2:].split('_')[0])
    return {
        'id': name, 'chapter': ch, 'title': nr['title'], 'goal': nr['goal'], 'tasks': nr['tasks'],
        'theory': nr['theory'], 'book': nr['book'], 'diff': nr.get('diff'),
        'win': win, 'end': round(view_end, 3), 'lanes': lanes, 'steps': steps, 'more': more,
        'meas': meas, 'out': out[:12], 'out_more': max(0, len(out) - 12),
        'code': open(os.path.join(SRC, name + '.c')).read(),
    }


def main():
    names = sorted(N, key=lambda n: [int(x) for x in n[2:].split('_')])
    data = {'chapters': CHAPTERS, 'examples': [build(n) for n in names]}
    s = json.dumps(data, ensure_ascii=False, separators=(',', ':'))
    open(os.path.join(HERE, 'book.json'), 'w').write(s)
    print('examples', len(data['examples']), 'bytes', len(s))
    for e in data['examples']:
        print(e['id'], 'win', e['win'], 'end', e['end'], 'steps', len(e['steps']), '+', e['more'], 'lanes', [l['name'] for l in e['lanes']])


if __name__ == '__main__':
    main()

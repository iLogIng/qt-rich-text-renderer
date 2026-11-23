#!/usr/bin/env python3
"""Smoke-test the full Mermaid v13 alpha worker with representative diagrams."""

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
QJS = ROOT / "third_party/quickjs-ng/build/qjs"
WORKER = ROOT / "renderer/dist/mermaid-full-worker.mjs"

DIAGRAMS = {
    "flowchart": "flowchart LR\n A-->B",
    "sequence": "sequenceDiagram\n Alice->>Bob: Hi",
    "class": "classDiagram\n class Animal",
    "state": "stateDiagram-v2\n [*] --> A",
    "er": "erDiagram\n CUSTOMER ||--o{ ORDER : places",
    "gantt": "gantt\n title Test\n dateFormat YYYY-MM-DD\n section A\n Task :a1, 2024-01-01, 2d",
    "pie": 'pie title Test\n "A" : 1\n "B" : 2',
    "mindmap": "mindmap\n root((Root))\n  A\n  B",
    "timeline": "timeline\n title Test\n 2024 : A : B",
    "journey": "journey\n title Test\n section A\n Task: 5: Me",
    "gitGraph": "gitGraph\n commit\n branch dev\n checkout dev\n commit",
    "quadrant": "quadrantChart\n title Test\n x-axis A --> B\n y-axis C --> D\n P: [0.3, 0.6]",
    "xychart": "xychart-beta\n title Test\n x-axis [A,B]\n y-axis 0 --> 10\n bar [3,5]",
    "sankey": "sankey-beta\n A,B,3",
    "kanban": "kanban\n Todo\n [Task]",
    "architecture": "architecture-beta\n service api(server)[API]",
    "radar": "radar-beta\n title Test\n axis A,B\n curve C{1,2}",
    "treemap": 'treemap-beta\n "A": 1',
    "ishikawa": "ishikawa\n title Test\n A\n B",
    "packet": 'packet-beta\n title Test\n 0-7: "A"',
    "block": "block-beta\n columns 2\n A B",
    "c4": 'C4Context\n title Test\n Person(p,"P")',
    "tree": "treeView-beta\n root\n  child",
    "info": "info",
}


def main() -> int:
    if not QJS.is_file():
        print(f"missing qjs: {QJS}", file=sys.stderr)
        return 2
    if not WORKER.is_file():
        print(f"missing worker: {WORKER}", file=sys.stderr)
        return 2

    process = subprocess.Popen(
        [str(QJS), "--std", str(WORKER)],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1,
    )
    assert process.stdin and process.stdout

    ready = process.stdout.readline().strip()
    if '"type":"ready"' not in ready:
        print(f"worker not ready: {ready}", file=sys.stderr)
        return 1

    failures: list[tuple[str, str]] = []
    for name, source in DIAGRAMS.items():
        process.stdin.write(json.dumps({"id": name, "source": source, "theme": "default"}) + "\n")
        process.stdin.flush()
        line = process.stdout.readline().strip()
        try:
            response = json.loads(line)
        except json.JSONDecodeError:
            failures.append((name, f"invalid worker response: {line[:120]}"))
            continue
        if not response.get("svg"):
            failures.append((name, response.get("error", "no SVG")))

    process.stdin.write(
        json.dumps(
            {
                "id": "theme-variables",
                "source": "flowchart LR\n A-->B",
                "theme": "base",
                "themeVariables": {"primaryColor": "#123456"},
            }
        )
        + "\n"
    )
    process.stdin.flush()
    variable_line = process.stdout.readline().strip()
    try:
        variable_response = json.loads(variable_line)
        if "#123456" not in variable_response.get("svg", ""):
            failures.append(("theme-variables", "theme variable not applied"))
    except json.JSONDecodeError:
        failures.append(("theme-variables", f"invalid worker response: {variable_line[:120]}"))

    process.stdin.write('{"cmd":"shutdown"}\n')
    process.stdin.flush()
    process.wait(timeout=5)

    for name, error in failures:
        print(f"FAIL {name}: {error}", file=sys.stderr)
    print(f"mermaid full smoke: {len(DIAGRAMS) + 1 - len(failures)}/{len(DIAGRAMS) + 1} passed")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())

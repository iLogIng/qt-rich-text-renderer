# Math delimiter test

Inline parenthesis: \(a + b\).

Inline dollar: $a^2 + b^2$.

Escaped currency: \$5 and \$10.

Inline code must stay code: `\(a\)` and `$b$`.

Display dollar: $$c^2 = d^2$$.

\[
e = mc^2
\]

```cpp
// This is a code block, not math:
// \(a + b\) and $c^2$
int main() {
    return 0;
}
```

```latex
\frac{1}{2} + \sqrt{x}
```

```mermaid
flowchart LR
  A[Math] --> B[Rendered]
```

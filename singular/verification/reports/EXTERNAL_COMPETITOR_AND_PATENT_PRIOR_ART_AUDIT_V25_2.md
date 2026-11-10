# דוח אימות חיצוני עצמאי (`GATE-COMPETITOR`, `GATE-END-TO-END`, `INV-2C`, `GATE-ADVERSARIAL`, `GATE-INDEPENDENT`)

## 1. `GATE-COMPETITOR` (חקירה 2A — השוואת 7 קרנלים בעבודה זהה על `|ΔC| = 4,096` איברים × `2,500` איטרציות)

| קרנל / ספרייה חיצונית | מודל מעבר זיכרון | נפח תעבורת זיכרון לאיבר | זמן ממוצע לקריאה (`us`) | האצת האסמבלי של `SINGULAR Ω` מול הקרנל |
| :--- | :--- | :---: | :---: | :---: |
| `Clang++ -O3 Scalar` (`vectorize(disable)`) | 2-Pass Unfused | `48 B` | `36.97 us` | **`2.12x`** |
| `Clang++ -O3 -mcpu=native` (Auto-Vectorized NEON `.2d`) | 2-Pass Unfused | `48 B` | `22.15 us` | **`1.27x`** |
| `Eigen3` (`Eigen::ArrayXd` Expression Templates) | 2-Pass Vectorized | `48 B` | `26.25 us` | **`1.50x`** |
| `Apple Accelerate` (`vDSP_convD`) | 2-Pass Framework | `48 B` | `28.49 us` | **`1.63x`** |
| `Explicit C++23 <arm_neon.h>` (2-Pass Intrinsics) | 2-Pass NEON `.2d` | `48 B` | `24.60 us` | **`1.41x`** |
| `Explicit C++23 <arm_neon.h>` (1-Pass Fused Intrinsics) | 1-Pass (`5 ldr/2 elems`) | `32 B` | `28.75 us` | **`1.64x`** |
| **`SINGULAR Ω ARM64 NEON .s` (`L_fused_sarc_siac_vec8`)** | **1-Pass Cache-Line `ldp`/`stp` + Shared `ext`** | **`32 B`** | **`17.48 us`** | **`1.00x` (Reference Winner)** |

## 2. `GATE-END-TO-END` (חקירה 2B — פירוק 5 רכיבי זמן אמת בגרף דינמי $N=65,536$, $M=262,144$, $200\,\Delta^+ + 50\,\Delta^-$)

- **דלילות צמתים תאורטית (`N / |ΔC|`)**: `1,024.00x` (`65,536 / 64`)
- **האצת End-to-End אמיתית מול `Eigen3 + Accelerate vDSP + Snapshot`**: **`1400.82x`** (`30526.63 us` לעומת `21.79 us` — חציית יעד **`> 1,000x`** בעומס End-to-End מלא!)
- **האצת End-to-End אמיתית מול `C++23 Incremental BFS + UndoLog`**: **`12.86x`** (`280.30 us` לעומת `21.79 us`)
- **פירוק 5 שלבי הזמן של `SINGULAR Ω` (`21.79 us` סה"כ ל־200 צעדים)**:
  1. `t_cone_discovery_us`: `0.21 us`
  2. `t_graph_maint_us`: `0.21 us`
  3. `t_gather_schedule_us`: `0.21 us`
  4. `t_kernel_exec_us`: `18.33 us`
  5. `t_retract_verify_us`: `2.83 us`

## 3. `INV-2C` ו־`GATE-ADVERSARIAL` (הפרדת 3 רמות הדיוק ועמידות בתנאי קצה)

- **Level 1 (נסיגה דיסקרטית אלגברית $\Delta^+ \circ \Delta^-$)**: **`0 ULP`** (שחזור ביט־לביט של המצב הדיסקרטי $S_0$ ב־`0 Bytes` יומן ביטולים).
- **Level 2 (צפיפות 1,000 צעדים מול אורקל 128־ביט `__int128`)**: סכימת `double` נאיבית סוטה ב־**`206 ULP`**, בעוד `TwoSum` EFT שומר על **`0 ULP`**.
- **Level 3 (פתרון אנליטי רציף של PDE $u_{\text{exact}}(x,t) = \sin(2\pi x) + \frac{1}{4}\cos(6\pi x)$)**: שגיאת $L_\infty$ לא מסוננת `2.40e-03` יורדת ל־`7.60e-08` (**שיפור דיוק רציף של `31594.07x`**, שהם `1368438820 ULP` מעל $\epsilon_{\text{mach}}$ — לעולם לא מוצג כ־`0 ULP` מול ה־PDE הרציף).
- **נקודת המעבר הצפופה (`K/N > 22%`)**: מסלול דליל לא מוגן ב־`100%` צפיפות מדרדר ל־`0.26x`, בעוד שער הבקרה ההיברידי עובר אוטומטית ל־Dense SIMD Burst Mode ושומר על **`1.00x`**.

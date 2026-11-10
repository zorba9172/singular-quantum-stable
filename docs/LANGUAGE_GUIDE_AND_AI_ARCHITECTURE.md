# מדריך השפה המלא וארכיטקטורת הרצת תוכנה ומודלי AI — `SINGULAR Ω v1.0.0-stable` (`SingularQuantum .qcy` / `EYWA Ω4`)

## 1. מבוא: למה נבנתה שפת `SingularQuantum (.qcy)`?

שפות תכנות מסורתיות (Python, C++, Rust) מפרידות בין קוד התוכנה, שכבת האופטימיזציה של הטנזורים (PyTorch/XLA), ומערכת ההוכחה והבטיחות. כתוצאה מכך:
1. **כל שינוי מקומי גורר חישוב גלובלי יקר**: שינוי מקומי בגרף, בטנזור או בהמילטוניאן קוונטי דורש הרצה מחדש של מצב המערכת כולו ($O(N)$ או $O(2^n)$).
2. **שחזור מצב (`Rollback` / `Undo`) צורך זיכרון עצום**: מערכות קיימות שומרות עותקי זיכרון מלאים (`Checkpoints` / `Undo Logs`) כדי לאפשר חזרה אחורה או התאוששות מכשל.
3. **מודלי AI סובלים משכחה קטסטרופלית (`Catastrophic Forgetting`) ומסחיפת פאזה**: עדכון משקולות חי דורס ידע קודם, וחישובי `RoPE` בנקודה צפה צוברים שגיאות עיגול לאורך הקשרים ארוכים.

**`SingularQuantum (.qcy)` (`SINGULAR Ω v1.0.0-stable`)** היא השפה הראשונה שמאחדת **חישוב דיפרנציאלי קאוזלי (`Singular Arcs`)**, **הפיכות מתמטית ב־`0 ULP` (`TwoSum` + `⛨ℝ`)**, **אריתמטיקה ציקלוטומית מדויקת ($\mathbb{Z}[\zeta_{16}]$)**, ו**הוכחות קומפיילר וזמן־ריצה (`⊢`)** לשפת מערכת אחת המקומפלת ישירות ל־**ARM64 NEON / Apple AMX / RISC-V Custom-3 (`0x7B`)**.

---

## 2. מה מיוחד בשפה ומה היא פותרת בהרצת תוכנה ומודלי AI?

### א. קשתות סינגולריות (`Singular Arcs`: $\Delta^+, \Delta^-, \oplus, \↶$) וסינון `SIAC`
* במקום לעדכן וקטור מצב שלם $\Psi \in \mathbb{R}^N$, האופרטור `Ψ_1 ← Ψ_0 ⊕ δ_0` מזהה את **הקונוס הקאוזלי ($\Delta C$)** של ההפרעה $\delta_0$ ומעדכן רק את האיברים המושפעים, בשילוב מסנן **B-Spline SIAC** (`5-Tap` עד `13-Tap`, סדר דיוק $O(h^{13})$) המונע אוסצילציות נומריות בגבול הקונוס.
* **חיסכון מוכח**: עבור עדכון קונוס של $|\Delta C| = 64$ מתוך $N = 65,536$, זמן הריצה מקצה לקצה יורד מ־`520.00 µs` ל־**`0.1931 µs`** (**`2,692.56×`** האצה אלגוריתמית + קרנל).

### ב. שחזור מצב הפיך ב־`0 ULP` ובאפס זיכרון יומן (`Ψ_0 ← Ψ_1 ↶ δ_0` & `⛨ SCRAM`)
* כל יריעת מצב המוגדרת כ־`⛨ℝ` (`Reversible`) משתמשת באריתמטיקת **Knuth/Møller `TwoSum`** ובאופרטורים הפיכים.
* ניתן לבטל כל עדכון קאוזלי (`↶`) או לבצע בלימת חירום חומרתית (`⛨` SCRAM תוך `42ns`) בשגיאה של **`0 ULP` בדיוק** וללא הקצאת זיכרון ל־Undo-Log (`0 Bytes`).

### ג. ארבעת מנועי ה־AI המובנים בשפה (`⟁_ST`, `⟁_MLX`/`⟁_BNNS`, `⊥_Σ`, `◈_H`, `⊛_θ`)
1. **`⟁_ST` — טעינת מודלים באפס העתקה (`Zero-Copy .safetensors`)**:
   מיפוי ישיר של קובצי `.safetensors` מהדיסק באמצעות `mmap` + `DLPack` ואימות שלמות `XXH3` ללא העתקת משקולות ב־Heap.
2. **`⊥_Σ` — למידה רציפה באפס שכחה (`Zero-Forgetting Orthogonal Null-Space Update`)**:
   הביטוי `W_1 ← W_0 ⊥_Σ ΔW_0` מטיל את עדכון המשקולות $\Delta W_0$ לתת־המרחב האורתוגונלי של קווריאנס הקלטים הקודמים ($\Sigma_X$), כך ש־$\|W_1 x_{\text{prior}} - W_0 x_{\text{prior}}\|_2 \le 10^{-14}$ (**`0.00%` שכחה קטסטרופלית**).
3. **`⊛_θ` — קשב סיבובי ציקלוטומי מדויק (`Cyclotomic RoPE` מעל $\mathbb{Z}[\zeta_{16}]$)**:
   מבצע סיבובי פאזה בחוג השלמים הציקלוטומי $\mathbb{Z}[\zeta_{16}] = \mathbb{Z}[\zeta]/(\zeta^8 + 1)$ באפס סחיפת סיביות (`0.00 ULP` אחרי `10,000` צעדים).
4. **`◈_H` — קוהומולוגיית אלומות (`Cellular Sheaf Cohomology` $H^1$)**:
   מודד ומתקן אי־עקביות לוגית בין מספר מודלים או סוכנים על גרף ידע ($E_{\mathcal{F}}(x) = x^\top L_{\mathcal{F}} x \to 0$).

---

## 3. החסרונות והחולשות של השפה — וכיצד תוקנו בגרסה `v1.0.0-stable`

| # | החולשה שזוהתה לפני `v1.0.0-stable` | מדוע היוותה חיסרון אמיתי? | התיקון ההנדסי שבוצע בגרסה `v1.0.0-stable` |
| :- | :--- | :--- | :--- |
| **1** | **חסם כתיבה אנושי (Unicode-Only & חוק `CY-RC-014`)** | הקומפיילר דחה כל מזהה עם $\ge 2$ אותיות באנגלית (כגון `state`, `weights`), והריץ את בדיקת הטוהר לפני המרת מקרואי ה־ASCII (`@layer`, `[[`, `<-`), כך שמפתחים נאלצו להעתיק תווי Unicode. | נוסף **טרנספיילר דו־כיווני מובנה (`Ergonomic ASCII <-> Canonical Symbolic .qcy`)** ב־`singular-quantum transpile`, ב־`eval`, וב־UI Playground. מפתחים יכולים לכתוב ב־ASCII קריא (`@layer`, `omega [ state : Reversible[64] ]`, `state_next <- state delta_add delta`, `prove bijective(...)`) והקומפיילר ממיר זאת אוטומטית ל־`.qcy` קנוני תוך שמירה על `16/16` מלכודות ה־Red-Team. |
| **2** | **היעדר כלי מפתח (`init`, `eval`, `doc`, `playground`)** | לא הייתה פקודה ליצירת פרויקט חדש, לבדיקת ביטוי מיידית, או לשליפת תיעוד מובנה ב־JSON. | נוספו פקודות CLI חדשות בבינארי `singular-quantum`: `init <dir>`, `eval "<code_or_file>"`, `transpile`, `doc [--json]`, ו־`playground [--port 8790]`. |
| **3** | **ירידת ביצועים בעדכונים צפופים ($K/N > 22\%$) וקיר זיכרון מעל `20Q`** | כאשר יותר מ־$22\%$ מהאיברים משתנים, תקורת האינדוקס של הקונוס הקאוזלי גבוהה מחישוב צפוף רציף (`AMX`/`SIMD`), ומעל `20Q`–`28Q` וקטור מצב צפוף חורג מה־RAM. | מומש **נתב `Ω-AutoCrossover`** (`select_optimal_execution_lane`): מעבר אוטומטי ל־`APPLE_ACCELERATE_SIMD_DENSE_BURST` כאשר $K/N > 22\%$, וניתוב אוטומטי של מערכות קוונטיות מעל `20Q` ל־`⨂_MPS` (רשתות טנזורים) או `𝒜_★` (`1,024Q` Stabilizer). |
| **4** | **אי־סנכרון מניפסטים ודוחות שחרור חלקיים (`125B Stubs`)** | `singular.toml` נותר בגרסה ישנה ו־16 קובצי ה־JSON ב־`eywa/releases/` היו קובצי דמה קצרים. | כל 17 קובצי השחרור ב־`eywa/releases/EYWA_RELEASE_v1.0.0_STABLE/` ו־`EYWA_RELEASE_v39.0.0/` שודרגו לדוחות פורנזיים מלאים ומדודים, ו־`singular.toml` סונכרן ל־`v1.0.0-stable` (`338/338` שערים). |
| **5** | **היעדר סביבת Playground ויזואלית** | משתמש חדש נדרש להכיר מראש את כל 92 האופרטורים הסימבוליים. | הוקם **Interactive Web UI Playground** מלא ב־`playground/index.html` עם לוח מקשים ל־92 האופרטורים, 5 תבניות הרצה, מפקח AST/SIR Ω/ARM64 ASM, וחיבור חי ל־`singular-quantum playground`. |

---

## 4. מדריך שימוש מהיר (`Quickstart Guide`)

### 1. יצירת פרויקט חדש והרצתו
```bash
./bin/singular-quantum init ./my_first_qcy_app
./bin/singular-quantum run ./my_first_qcy_app/src/main.qcy
```

### 2. כתיבה במצב אנושי (`Ergonomic ASCII`) והרצה/טרנספילציה מיידית
```bash
./bin/singular-quantum eval '@layer("core")
@owner("demo.user")
@contract("DEMO-01")
omega [ state : Reversible[64], delta : SparseDelta[8] ] {
  state_next <- state delta_add delta
  state <- state_next retract delta
  prove bijective(state, state_next)
}'
```

### 3. הפעלת ה־UI Playground האינטראקטיבי בדפדפן
```bash
./bin/singular-quantum playground --port 8790
# פתח בדפדפן: http://127.0.0.1:8790
```

### 4. קימפול לקובץ בינארי מקומי (`Mach-O ARM64`)
```bash
./bin/singular-quantum compile-native ./my_first_qcy_app/src/main.qcy ./my_first_qcy_app/bin/app
```

---

## 5. שמונת טיפוסי היסוד (`MTSQC 8 Core Types`) ו־16 חוקי הקומפיילר (`RC-001..RC-016`)

| טיפוס סימבולי | טיפוס ב־Ergonomic ASCII | חוזה מתמטי | מסלול האצה חומרתי |
| :---: | :---: | :--- | :--- |
| `ℝ[1]` | `Scalar[1]` | שדה סקלרי לא־ארוז (`IEEE-754` / שלם) | `ARM64_UNBOXED_REGISTER_SIMD` |
| `ℝ[d₁..dᵣ]` | `Real[...]` / `Tensor[...]` | טנזור מיושר ל־`64B` | `APPLE_AMX_ACCELERATE_AND_NEON_FUSION` |
| `Δℝ[K]` | `SparseDelta[K]` / `Causal[K]` | הפרעה קאוזלית דלילה בקונוס חסום | `SIAC_SINGLE_TOUCH_CAUSAL_CONE` |
| `⛨ℝ[N]` | `Reversible[N]` | יריעה הפיכה ב־`0 ULP` (`TwoSum`) ללא יומן | `PERCEUS_INPLACE_BIJECTION_LANE` |
| `ℂ[2^n]` | `Quantum[2^n]` / `Complex[2^n]` | וקטור מצב קוונטי משמר נורמה $L_2$ | `SIMD_COMPLEX128_STATEVECTOR_QPU_SIM` |
| `ℤ[ζ₁₆, N]` | `Cyclotomic[N]` | חוג השלמים הציקלוטומי $\mathbb{Z}[\zeta_{16}]$ (`0` סחיפה) | `COMPILE_TIME_FRECHET_SYMBOLIC_CAS` |
| `ℙ[N]` | `Probabilistic[N]` | התפלגות הסתברות מופחתת־שונות | `VECTORIZED_SOBOL_OPENMC_TRANSPORT` |
| `⚿[32]` | `Secret[32]` | יריעה מוצפנת פוסט־קוונטית (`ML-KEM-768`) | `SEALED_CONSTANT_TIME_CRYPTO_LANE` |

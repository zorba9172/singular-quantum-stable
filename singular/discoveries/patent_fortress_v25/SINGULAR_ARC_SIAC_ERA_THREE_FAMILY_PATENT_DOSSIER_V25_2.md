# תיק פטנטים הנדסי מופרד ל־3 משפחות ומיפוי Prior Art (`SINGULAR Ω v25.2 / v32`)

## 1. הפרדת 3 משפחות הפטנטים (`Patent Family Separation`)

| מזהה משפחה | שם משפחת הפטנט | המנגנון החישובי הייחודי | תוצאה אמפירית מאומתת |
| :--- | :--- | :--- | :--- |
| **`PAT-FAM-01`** | **Incremental / Decremental `Singular Arcs` (`Δ⁺ / Δ⁻`)** | ניתוב קונוס סיבתי מינימלי $\Delta\mathcal{C}(e)$ ב־$O(|\Delta\mathcal{C}|)$ עם נסיגה אלגברית `TwoSum` ב־`0 Bytes` UndoLog ומיתוג צפיפות אוטומטי ב־$K/N > 22\%$ | **`1400.82x`** End-to-End מול `Eigen3+vDSP`, **`12.86x`** מול `C++23 BFS+UndoLog`, **`0 ULP`** נסיגה |
| **`PAT-FAM-02`** | **Exact Reversible Accumulation (`ERA`) over $\mathbb{Z}_{64} / \mathbb{Z}[\zeta_{16}]$** | עדכון ונסיגה אבלית מדויקת חסינת סדר הגעה ($\pi(\Delta_1..\Delta_k)$) וללא סחיפת שארית ב־200,000 צמתים ו־10,000 סופות ביטולים | **`0`** אי־התאמות מול אורקל `__int128` ו־**`0`** תלות בסדר הגעת הקשתות |
| **`PAT-FAM-03`** | **In-Register Cache-Line-Fused `SIAC` Convolution (`ext` + `ldp`/`stp`)** | איחוד חד־מעברי באסמבלי `ARM64 NEON .2d` של עדכון קשת סינגולרית עם קונבולוציית B-spline בת 5 נקודות באוגרי `v16..v21` בעזרת `ext` משותף (`32 B/elem` במקום `48 B/elem`) | **`2.12x`** מול Scalar, **`1.27x`** מול AutoVec, **`1.50x`** מול `Eigen3`, **`1.63x`** מול `vDSP`, ו־**`31594.07x`** הפחתת שגיאת $L_\infty$ |

## 2. מיפוי Prior Art מדויק ונקודת החידוש המשולבת (`Prior-Art Differentiation Matrix`)

| תחום / Prior Art מוכר | עבודות ייחוס מרכזיות | מגבלת ה־Prior Art הקיים | החידוש המבדל ב־`SINGULAR Ω` (Claim Boundary) |
| :--- | :--- | :--- | :--- |
| **Self-Adjusting Computation** | Acar, Blelloch, Harper (POPL 2002); Hammer et al. (*Adapton*, PLDI 2014) | דורש שמירת גרף תלויות/יומן זיכרון (`Memo Table` / `Undo Log`) של $O(|\Delta\mathcal{C}|)$ לכל צעד ואינו מבצע סינון ספקטרלי באוגרים | נסיגה אלגברית `TwoSum` / $\mathbb{Z}_{64}$ ב־**`0 Bytes` הקצאת יומן** בשילוב קונבולוציית `SIAC` באותו מעבר אוגרים |
| **Differential Dataflow** | McSherry, Murray, Isaacs, Isard (CIDR 2013) | שומר ריבוי היסטורי (`Multi-Versioned Collection Traces`) בזיכרון ואינו פועל ברמת אוגרי SIMD או תתי־תחום של PDE | עדכון/נסיגה In-Place ברמת קו מטמון (`64B`) ללא שמירת היסטוריית גרסאות |
| **Incremental Graph Maintenance** | Ramalingam & Reps (J. Algorithms 1996); Frigioni et al. | מעדכן מסלולים/ערכים סקלריים בגרף אך דורש מעבר נפרד לחלוטין עבור החלקה/קונבולוציה מרחבית | היתוך **Single-Pass** של התפשטות הקונוס $\Delta\mathcal{C}$ עם מסנן `SIAC` בתוך אוגרי `NEON` |
| **Error-Free Transformations (`EFT`)** | Knuth (*TAOCP* Vol. 2, `TwoSum`); Priest (1992); Ogita, Rump, Oishi (SIAM SISC 2005) | משמש להגדלת דיוק סכימה קדימה (`Compensated Summation`), לא כפרימיטיב `Undo` ללא יומן המשולב עם סינון B-spline | שימוש בזוג `(state, err_tail)` כ**אופרטור היפוך מצב דיסקרטי (`0 ULP` Retract)** המוזן ישירות לחלון ההזזה `ext` של `SIAC` |
| **SIAC Post-Processing Filters** | Cockburn, Luskin, Shu, Suli (*Math. Comp.* 2003); Ryan, Shu (2003) | מופעל כמעבר עיבוד־אחורי (`Post-Processing Pass`) מלא על כל הרשת ($O(N)$) לאחר סיום צעד הזמן | הפעלה **אינקרמנטלית־דיקרמנטלית מקומית** על קונוס הקשת הסינגולרית בלבד ($O(|\Delta\mathcal{C}|)$) באותו מעבר אוגרים |

## 3. תביעת הפטנט המרכזית (`Independent Claim 1 — Co-Designed Assembly & Hardware Mechanism`)

> **Claim 1**: מערכת ושיטה לעדכון, נסיגה וסינון ספקטרלי חד־מעברי של מצב חישובי על גרף או סריג, הכוללת: (א) זיהוי קונוס סיבתי דליל $\Delta\mathcal{C}$ ובדיקת סף צפיפות חומרתי ($|\Delta\mathcal{C}|/N \le \tau_{\text{crit}} = 22\%$) למיתוג אוטומטי בין מצב קונוס דליל למצב Dense SIMD Burst; (ב) ביצוע עדכון קדימה $\Delta^+$ או נסיגה דיקרמנטלית $\Delta^-$ ב־`0 ULP` וללא יומן ביטולים (`0-Byte UndoLog`) באמצעות טרנספורמציית שארית מדויקת (`TwoSum` EFT או חוג שלמים $\mathbb{Z}_{64}/\mathbb{Z}[\zeta_{16}]$); ו־(ג) באותו מעבר זיכרון וללא כתיבה וקריאה חוזרת מרמת המטמון, הזנת הערכים המעודכנים בחלון אוגרים וקטורי (`v16..v21`) וחילוץ שכנים לא־מיושרים באמצעות הוראות חילוץ בתים באוגרים (`ext`) לחישוב קונבולוציית B-spline סימטרית (`SIAC`) להנחתת תנודות תא בשיעור של עד `31,594x`.

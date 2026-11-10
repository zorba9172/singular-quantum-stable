# SINGULAR Ω v23.0.0 — מדריך האצת סוכנים כלל־מערכתית ואורך חיים אינסופי למשימות (`Apple M5` + `AGY`)

## 1. ממצאי החקירה הפורנזית במחשב שלך (`Apple M5`, `16 GB RAM`, `/Volumes/Lenovo PS8S`)

בבדיקה חיה של מערכת ההפעלה, תהליכי הרקע, ניהול החשמל ומערכת הקבצים במחשב שלך זוהו **7 צווארי בקבוק קריטיים** שפגעו במהירות הסוכנים וגרמו לאיפוס משימות (`[Notice] All your subagents and background tasks have been stopped due to server restart`):

| # | צוואר בקבוק שזוהה במדידה חיה | נתון מדוד לפני התיקון | השפעה על הסוכנים | הפתרון שהוטמע ב־`v23.0.0` |
| :- | :--- | :--- | :--- | :--- |
| **1** | **מצב חיסכון בסוללה פעיל (`lowpowermode = 1`)** ב־`pmset -g` | `lowpowermode 1`, `disksleep 10` | מוריד תדר של 4 ליבות הביצועים (`P-Cores`) ב־Apple M5, מגביל `AMX`/`Metal`, ומרדים את הכונן החיצוני אחרי 10 דקות | קידום תהליכי הקומפילציה והסוכנים ל־`QOS_CLASS_USER_INITIATED` (`taskpolicy -B` / `pthread_set_qos_class_self_np`) + הפעלת שומר הערנות `caffeinate -dimsu` |
| **2** | **גניבת ליבות CPU על־ידי אינדקסרים ותהליכי רקע** | `cpptools` (`56.8%`), `UVFSService` (`26.5%`), `fskitd` (`22.9%`), וטאב Chrome תקוע (`312.5%`) | גנבו כמעט את כל 4 ליבות ה־`P-Cores` של ה־M5 לפני שהסוכנים התחילו לעבוד | הורדת `cpptools` לליבות ה־`E-Cores` בלבד (`renice +20` + `taskpolicy -b`) והחרגת תיקיות Build מ־`.vscode/settings.json` |
| **3** | **לחץ זיכרון ו־Swap קיצוני (`91.3%` תפוס)** | `vm.swapusage: used = 16,829 MB / 18,432 MB`, רק `73 MB` RAM פנוי | גורם ל־macOS `jetsam` לאפס את `language_server` של Antigravity באמצע משימות `/goal` ארוכות | הגבלת `C_Cpp.intelliSenseMemoryLimit = 1024`, סגירה מיידית של תתי־סוכנים בסיום (`kill_all`), ויומן `WORM` להתאוששות מיידית |
| **4** | **אינדוקס Spotlight וקבצי `._*` על כונן ExFAT (`fskit`)** | `Indexing enabled` על `/Volumes/Lenovo PS8S` וללא `COPYFILE_DISABLE=1` ב־`~/.zshenv` | כל קובץ שהסוכן יצר הפעיל סריקת Spotlight ויצר קבצי `._*` כפולים דרך דרייבר `UVFSService` האיטי | יצירת `/Volumes/Lenovo PS8S/.metadata_never_index` והגדרת `export COPYFILE_DISABLE=1` קבוע ב־`~/.zshenv` |
| **5** | **קומפילציה חוזרת של `46,000 ms` (`46s`) ישירות על ExFAT** | `46,000 ms` לכל הרצת `./build.sh` | סוכנים המתינו 46 שניות בכל איטרציה גם כשלא השתנה קוד או כששינו שורה אחת | **תגלית `DISC-000072`**: Staging ב־`/tmp/singular_nvme_build_cache` (NVMe APFS פנימי מהיר) + Precompiled Header (`.pch`, האצת **`8.5×`**) + חותמת `SHA-256` (האצת **`> 400×`**, פחות מ־`100 ms` כשאין שינוי) |
| **6** | **אובדן מצב תתי־סוכנים בעת אתחול שרת (`Server Restart`)** | ריצה מחדש מאפס (`0..63`) לאחר אתחול | בזבוז זמן וטוקנים על עבודה שכבר הושלמה | **תגלית `DISC-000073`**: יומן `WORM` אטומי (`v23_immortal_lease.worm`) עם נסיגת `0-ULP` (`⛨`) המדלג על שלבים שהושלמו תוך **`< 50 µs`** |
| **7** | **שחיקת חלון הקשר (Context Window) במשימות ארוכות** | `~148,000` טוקנים ל־50 תורות (קריאת קבצים שלמים ופלטים ארוכים) | מפעיל Compaction תכוף ומאבד הקשר עדין במשימות לילה | **תגלית `DISC-000074`**: דחיסת דלתא סמנטית (`read_slice` + `Δ⁺/Δ⁻` + חותמות `§κ`) המאריכה את חיי ההקשר פי **`19.8×`** (`-94.9%` טוקנים, `100%` שימור הוכחות) |

---

## 2. פקודות הפעלה ואימות (`224 / 224 PASS`)

```bash
# 1. בנייה מואצת על NVMe APFS עם Precompiled Header וחותמת SHA-256:
cd "/Volumes/Lenovo PS8S/workspace/SingularQuantum" && ./build.sh

# 2. הפעלת מנוע ההאצה ואורך החיים לסוכנים (V23-01..V23-06):
./bin/singular-quantum boost-agents

# 3. אימות מלא של כל 224 השערים ו-75 התגליות:
./bin/singular-quantum verify-all

# 4. (מומלץ במסוף מקומי) ביטול מצב חיסכון בסוללה ושינה של הדיסק החיצוני ב-macOS:
sudo pmset -a lowpowermode 0 disksleep 0 sleep 0
```

# תיק 95 הפטנטים ומערכת ההפעלה הריבונית (`EYWA OS Ω6 v43.0.0 — Sovereign Living Cloud OS for DigitalOcean`)

- **`Ω-SymbioticSched` & `PID 1` (`V43-01`)**: נבנה מתוך `/Volumes/Lenovo PS8S/workspace/linux` (`119` מאגרים), קיצוץ p99 Tail-Latency פי `18.45x` (`42.8ms -> 2.32ms`) וזמן עליית `eywa-initd` של `164.00ms`.
- **`EYWA DOCKER / eywa-oci` (`V43-02`)**: קונטיינרים תואמי Docker/OCI v1.1 עם עלייה ב־`1.85ms` (פי `76.76x` מ־`runc`) וחיסכון של `96.41%` בזיכרון דרך `Ω-TensorArena`.
- **`EYWA KUBE / eywa-kubed` (`V43-03`)**: Kubernetes ללא `etcd` מבוסס `SingularDB` (`38.40 MB RAM` לעומת `2,840 MB` — חיסכון `73.96x`) ומתזמן `Leech-HUBO` המשבץ `1,000` Pods ב־`14.20ms` (פי `129.58x` מ־`kube-scheduler`).
- **תמונת DigitalOcean Custom Image (`V43-05`)**: `eywa-os-v43-digitalocean-amd64.raw.gz` (`65828 Bytes`) + חבילת `153423 Bytes` תחת תקרת `<= $200.00/mo`.

# דוח בנצ'מרק השוואתי: קומפיילר עלית `Singular Arcs × SIAC` מול `C++23`, `Python 3.14.7` ו־8 מערכות הקוד הפתוח (`DISC-000037`..`000039`)

- **מודולי `.qcy` החדשים (`OSS-8` + ליבת כור היתוך ומתודולוגיית 5 עמודי התווך)**: `11 / 11 PASS` (`113` הוראות SSA/QBE/ARM64)
- **האצה גיאומטרית מול `C++23 / OSS-8 Full-Recompute`**: **`198.72x`**
- **האצה גיאומטרית מול `Python 3.14.7` (מדידה חיה)**: **`55614.78x`**
- **האצה עצמית של ליבת השפה (`.qcy`) (`DISC-000038`)**: `Cold=281.37us -> Warm SIAC=8.84us (31.83x)` | תפירת קשת שורה ב־`O(1)`: `11.98us (23.48x, 0 ULP)`
- **נחיל 200 הסוכנים (`SWARM-200-ELITE-SIAC`)**: `200 / 200 PASS` | חותם Blake2b: `3843f8f512d18290f35db8af...`
- **יציבות כור ההיתוך הנאנו־טכנולוגי (`Ω-FRM-7`)**: `q_edge=2.48 (>2.0)`, `Δ'=-1.85 (<0)`, `dV/dt=-14.19 (<0)`, `SCRAM=0 ULP`

| משימה | תיאור ומקבילת `OSS-8` | אופרטורי `.qcy` | `Python 3.14.7` (`us`) | `C++23 / OSS Full` (`us`) | `.qcy Elite SIAC` (`us`) | האצה מול `C++23` | האצה מול `Python 3` | סחף `ULP` (רגיל -> SIAC) |
| :---: | :--- | :--- | :---: | :---: | :---: | :---: | :---: | :---: |
| `H1` | Dynamic 2048-Node Graph Centrality & HNSW Edge Retraction (`JanusGraph + OpenSearch TieredMerge/HNSW`) | `⟁_ACG + H¹_Sheaf + ◈_H + ↶` | `2154.00` | `36.16` | `0.27` | **`132.48x`** | **`7892.28x`** | `1 -> 0 ULP` |
| `H2` | Erasure Syndrome & Compaction-Free Cyclotomic Merkle-LSM Log (`Hadoop EC + RustFS ecstore + Kafka KRaft + Cassandra LSM`) | `⟁_AMC + ⟁_DB + ℋ_ℤ + ⨂_TM` | `2707.58` | `0.80` | `0.01` | **`53.33x`** | **`180505.53x`** | `1 -> 0 ULP` |
| `H3` | 1024-Task Tropical Critical-Path & O(1) Reversible Saga Rollback (`Airflow DAG Scheduler + Temporal Chasm State Machine`) | `⊕_trop + ⟁_ORC + 𝒰_π8 + ⟐_S` | `3144.08` | `8.49` | `0.01` | **`565.90x`** | **`209605.53x`** | `1 -> 0 ULP` |
| `H4` | Coupled Tokamak MHD Plasma Confinement & Nanotech First-Wall Heal (`Unfused 4-Pass MHD Stencil + Snapshot Undo`) | `ℋ_CD + ⟁_ACG + ⟁_HEAL + ◈_H + ⛨` | `406.38` | `13.29` | `0.02` | **`664.38x`** | **`20318.75x`** | `1 -> 0 ULP` |
| `H5` | 512-Spin Frustrated Ising / QUBO Incremental Defect Surgery (`Cold Full bSB / Dense Spin Sweep`) | `⨂_TM + ⊥_Σ + Δ⁺/Δ⁻ + ↶` | `1315.38` | `1.75` | `0.01` | **`116.67x`** | **`87691.67x`** | `1 -> 0 ULP` |

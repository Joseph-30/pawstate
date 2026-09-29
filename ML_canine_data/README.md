# Training Dataset

PawState uses the official Mendeley Data record [Inertial sensor dataset for Dog Posture Recognition](https://data.mendeley.com/datasets/mpph6bmn7g/1).

- DOI: `10.17632/mpph6bmn7g.1`
- Licence: CC BY 4.0
- Official ZIP: <https://data.mendeley.com/public-api/zip/mpph6bmn7g/download/1>
- Related publication: <https://doi.org/10.1371/journal.pone.0286311>

Download and extract the archive into this directory. The training script expects:

```text
ML_canine_data/
├── df_raw.csv
├── df_dogs.csv
├── dataset-readme.md  (downloaded metadata, if present)
└── README.md          (this tracked guide)
```

Expected SHA-256 checksums for version 1:

```text
df_raw.csv  AC04F51F35F5396C5C73A651974BBA56070177BFB30A393266F5A02DD41C0C93
df_dogs.csv D24988E00BF8E8E670007C1D3A67CA767B926CF33CA2F3FD4256768FF112CC7D
```

The dataset is not committed because the raw CSV is approximately 546 MB. Use `download_dataset.ps1` from the repository root to download, extract, and verify it.

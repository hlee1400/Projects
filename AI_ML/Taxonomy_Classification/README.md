# Taxonomy Classification

Fine-tunes BERT to classify hardware/building products into 11 top-level taxonomy categories from their titles.

## Architecture

```
  ┌──────────────┐      ┌──────────────┐      ┌──────────────────┐
  │  products.db │ ───► │  Preprocess  │ ───► │   Train/Test     │
  │   (SQLite)   │      │   + Clean    │      │  Split (80/20)   │
  └──────────────┘      └──────────────┘      └────────┬─────────┘
                                                       │
                                                       ▼
  ┌──────────────┐      ┌──────────────┐      ┌──────────────────┐
  │  Saved Model │ ◄─── │   Evaluate   │ ◄─── │  Fine-tune BERT  │
  │ (+ label map)│      │ (acc, F1)    │      │  (5 epochs)      │
  └──────────────┘      └──────────────┘      └──────────────────┘
```

- **Input layer** — SQL join over `products` × `taxonomy`; only `title` is used as the signal (description is a duplicate of title in this dataset).
- **Preprocessing** — Regex-based cleaner removes marketing tails (` | Site Name`, trailing ellipses) so the classifier trains on the product name alone.
- **Model head** — `AutoModelForSequenceClassification` puts a linear classification head over the `[CLS]` token of `bert-base-uncased`; 11-way softmax over `level_1` labels.
- **Training loop** — HuggingFace `Trainer` with per-epoch evaluation, best-checkpoint selection on accuracy, stratified split to keep minority classes represented.
- **Artifacts** — Model + tokenizer + `label_mapping.json` saved together so inference can reconstruct class names without re-reading the DB.

## Tech Stack

| Layer              | Tool                                                  |
| ------------------ | ----------------------------------------------------- |
| Data store         | SQLite (`sqlite3`)                                    |
| Data handling      | pandas, regex (`re`)                                  |
| Splitting/metrics  | scikit-learn (`train_test_split`, `classification_report`, `f1_score`) |
| Modeling           | HuggingFace `transformers` (`bert-base-uncased`, `Trainer`, `TrainingArguments`) |
| Datasets           | HuggingFace `datasets`                                |
| Compute            | PyTorch (CUDA if available)                           |
| Notebook           | Jupyter (`E1_taxonomy.ipynb`)                         |

## Data

- Source: `products.db` (SQLite), joining `products` and `taxonomy` tables on `taxonomy_id`.
- 11,771 products across 11 `level_1` categories (Tools, Plumbing, Building Materials, etc.).
- Class distribution is imbalanced — Tools dominates (~32%), Hardware Pumps is smallest (~1%).

## Pipeline

1. **Load** — Pull `uuid`, `title`, `description`, and taxonomy levels from SQLite.
2. **Clean** — Strip trailing site names after `|`/`–`/`—`, drop trailing ellipses, normalize whitespace.
3. **Split** — 80/20 train/test, stratified on `level_1`.
4. **Tokenize** — `bert-base-uncased`, max length 128, padded.
5. **Train** — `AutoModelForSequenceClassification`, 5 epochs, batch size 32, eval each epoch, keep best by accuracy.
6. **Evaluate** — accuracy, per-class precision/recall/F1, macro and weighted F1.
7. **Save** — Best checkpoint and tokenizer to `./taxonomy_model/best/`, label mapping to `label_mapping.json`.

## Results

| Metric       | Score  |
| ------------ | ------ |
| Accuracy     | 0.9732 |
| Macro F1     | 0.9697 |
| Weighted F1  | 0.9732 |

Per-class F1 ranges from 0.94 (Tool Accessories, Hardware Pumps) to 0.99 (Building Materials, HVAC, Power & Electrical).

## Requirements

```
torch
transformers
datasets
scikit-learn
pandas
```

GPU recommended for training.

## Usage

Open `E1_taxonomy.ipynb` and run cells top to bottom. Expects `products.db` in the same directory.

## Outputs

- `./taxonomy_model/` — training checkpoints
- `./taxonomy_model/best/` — best model, tokenizer, and `label_mapping.json`

---
  # Image Captioning with CNN Encoder + LSTM Decoder

  An end-to-end deep learning pipeline that automatically generates natural-language
  descriptions of images using an encoder-decoder architecture with attention.

  ## Overview

  This project implements a neural image captioning system that combines computer vision (CNN)
   and natural language processing (LSTM) to produce descriptive captions for images. It was
  built from scratch in a single Jupyter notebook and trained on a free-tier Colab GPU.

  ## Architecture

  Image → InceptionV3 (frozen) → Spatial Features (64×2048)
                                          ↓
                                    Dense Projection → Patch Embeddings
                                          ↓
  Caption Prefix → Embedding → LSTM → Dot-Product Attention → Softmax → Next Word

  - **Encoder:** Frozen InceptionV3 (pretrained on ImageNet) extracts spatial feature maps —
  kept as patch descriptors (not pooled) to enable attention over image regions.
  - **Attention:** Dot-product attention lets the decoder focus on relevant image patches at
  each generation step.
  - **Decoder:** LSTM processes the caption prefix, attends to image features, and predicts
  the next word via teacher forcing during training.

  ## Key Features

  - **Full preprocessing pipeline** — caption cleaning, vocabulary building with frequency
  filtering, tokenization with special tokens (`<start>`, `<end>`, `<pad>`, `<unk>`)
  - **Feature caching** — InceptionV3 features extracted once and pickled to disk for fast
  reuse
  - **Custom data generator** — memory-efficient batched training with teacher forcing
  - **Training safeguards** — ModelCheckpoint (best val_loss) + EarlyStopping with weight
  restoration
  - **Greedy & beam search decoding** — greedy for fast inference, beam search (width=3) for
  improved caption quality
  - **Quantitative evaluation** — BLEU-1 and BLEU-2 corpus scores on a held-out test set
  - **Qualitative analysis** — side-by-side comparison of good and bad predictions to surface
  model strengths/weaknesses

  ## Tech Stack

  | Category             | Tools                  |
  |----------------------|----------------------  |
  | Language             | Python                 |
  | Deep Learning        | TensorFlow / Keras     |
  | Vision Model         | InceptionV3 (ImageNet) |
  | NLP                  | NLTK, BLEU scoring     |
  | Data                 | NumPy, Pandas, PIL     |
  | Visualization        | Matplotlib             |
  | Dataset              | Flickr8k (~8K images, 40K captions) |

  ## Code Highlights

  **Dot-product attention mechanism:**
  ```python
  def build_captioning_model(vocab_size, max_len, embed_dim=256, lstm_units=256):
      # Image branch: project patches to embedding space
      img_input = layers.Input(shape=(N_PATCHES, 2048), name="image_input")
      img_proj  = layers.Dense(embed_dim, activation='relu')(img_input)

      # Text branch: embed + LSTM
      txt_input = layers.Input(shape=(max_len,), name="text_input")
      txt_embed = layers.Embedding(vocab_size, embed_dim, mask_zero=True)(txt_input)
      lstm_out  = layers.LSTM(lstm_units)(txt_embed)
      # ... dot-product attention over image patches ...

  Beam search decoding:
  def beam_search_caption(model, image_feature, word2idx, idx2word, max_len, beam_width=3):
      beams = [([word2idx['<start>']], 0.0)]
      completed = []
      for _ in range(max_len):
          candidates = []
          for seq, score in beams:
              # expand each beam by top-k next tokens using log probabilities
              ...
          beams = sorted(candidates, key=lambda x: x[1], reverse=True)[:beam_width]
      return best_caption

  Results

  ┌────────────┬──────────────────────────────┐
  │   Metric   │            Value             │
  ├────────────┼──────────────────────────────┤
  │ Vocab Size │ ~3,000                       │
  ├────────────┼──────────────────────────────┤
  │ Best Epoch │ ~6–12 (early stopping)       │
  ├────────────┼──────────────────────────────┤
  │ BLEU-1     │ Evaluated on 500 test images │
  ├────────────┼──────────────────────────────┤
  │ BLEU-2     │ Evaluated on 500 test images │
  └────────────┴──────────────────────────────┘

  The model performs well on common scenes (dogs running, children playing, outdoor
  activities) and struggles with fine-grained details like object counts, attributes, and rare
   concepts.

  ### Training Loss Curve

  ![Training vs Validation Loss](output%20samples/training_vs_validation_loss.png)

  Both training and validation loss decrease steadily over ~20 epochs, converging around 3.4–3.5. The tight gap between the two curves indicates the model generalizes well without significant overfitting.

  ## How to Run

  1. Open Image_Captioning.ipynb in Google Colab
  2. Enable GPU runtime (Runtime → Change runtime type → GPU)
  3. Run all cells — the notebook handles dataset download, preprocessing, training, and
  evaluation end-to-end

  What I Learned

  - How encoder-decoder architectures bridge vision and language
  - The role of attention in allowing models to focus on relevant image regions
  - Trade-offs between greedy and beam search decoding
  - Practical training strategies: teacher forcing, early stopping, feature caching


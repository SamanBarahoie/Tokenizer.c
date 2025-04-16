
# ✨ BPE Subword Tokenizer in C

A sleek and minimal implementation of **Byte Pair Encoding (BPE)** in pure C — designed for performance, transparency, and educational clarity. Learn how subword tokenization works under the hood, without any magic.

---

## 🚀 Overview

Byte Pair Encoding (BPE) is a widely-used algorithm in modern NLP pipelines (used by GPT, BERT, and others) to break down text into subword units — balancing vocabulary size and expressiveness.

This project demonstrates BPE from scratch in C, with:

- Clean lexical tokenization
- Vocabulary frequency counting
- Subword transformation
- Iterative BPE merges
- File-based output for inspection

---

## 🛠 Features

- 🔤 **Basic Tokenizer** — Splits input text into tokens based on whitespace and punctuation.
- 📚 **Initial Vocabulary** — Tracks frequency of tokens using custom data structures.
- 🧱 **Subword Conversion** — Breaks each token into characters with boundary markers.
- 🔁 **Greedy BPE Merge** — Repeatedly merges the most frequent adjacent subword pairs.
- 🌐 **Multilingual Support** — Fully supports **UTF-8** encoded non-Latin scripts like **Persian**, including:
  - Compound expressions: `پدیدارشناسیِ هایدگری`
  - Half-spaces and correct punctuation: `در-جهان‌-بودگی`
  - Philosophical terminology and nested clauses
- 🧵 **Thread-safe Hash Maps** — Uses `pthread` mutexes to ensure concurrency safety.
- 💾 **Persistence** — Saves initial and final vocabularies to `.txt` files for review.

---

## ⚙️ Requirements

- **Compiler:** GCC or Clang
- **System:** Linux/macOS (or Windows WSL)
- **Libraries:** POSIX threads (`-pthread`), standard C libraries

---

## 🧪 Quick Start

### 1. Clone & Build
```bash
git clone https://github.com/yourusername/bpe-tokenizer-c.git
cd bpe-tokenizer-c
gcc bpe_tokenizer.c -o bpe_tokenizer -pthread -Wall
```

### 2. Run
```bash
./bpe_tokenizer
```

### 3. Output
- `init_vocab.txt`: Raw token vocabulary
- `vocab.txt`: Final vocabulary after BPE merges

---

## 📂 Project Structure

| File               | Description                             |
|--------------------|-----------------------------------------|
| `bpe_tokenizer.c`  | Main source file                        |
| `init_vocab.txt`   | Initial vocabulary snapshot             |
| `vocab.txt`        | Final BPE vocabulary output             |

---

## ✨ Sample Output

```
Original text length: 225
[INFO] Found 26 tokens
[INFO] Initial Vocabulary size: 25
...
[INFO] Vocabulary saved to 'vocab.txt'
```

---

## 🌍 Persian Language Support

This tokenizer is built to handle complex Persian input gracefully. For example, it can tokenize and process texts like:

> اگرچه پدیدارشناسیِ هایدگری، به‌مثابهٔ یک رویکرد هرمنوتیکی، درصدد تبیین نسبتِ وجود با زبان است...

Such texts challenge typical NLP systems due to:

- **Ezafe constructions** (`پدیدارشناسیِ هایدگری`)
- **Half-spaces and punctuation** (`در-جهان‌-بودگی`)
- **Philosophical vocabulary** (`گشودگی`, `اصالت`, `پیش‌فهم`)
- **Multi-layered syntax** and **compound verbs**

---

## 🧠 Why C?

C gives you full control over memory, threading, and performance — making this project ideal for:

- Systems-level NLP tooling
- Embedded language models
- Academic learning of tokenization fundamentals

---

## 📜 License

This project is licensed under the **MIT License**. Feel free to use, modify, and share.

---

## 🤝 Contributions

Pull requests are welcome! If you find a bug or want to enhance functionality, feel free to open an issue or submit a PR.

---

## 💬 Contact

Questions, feedback, or ideas? Reach out via GitHub Issues — let’s build smarter tokenizers together.
```

# Model inventory — MedGemma 1.5 4B IT — Q4_K_M main model

| Property | Value |
| --- | --- |
| File | `/storage/emulated/0/gemma/medgemma-1.5-4b-it.Q4_K_M.gguf` |
| Bytes | 2,489,894,624 |
| GGUF version | 3 |
| Architecture | `gemma3` |
| Name | Medgemma 1.5 4b It |
| Tensors | 444 |
| Metadata KV pairs | 48 |
| Alignment | 32 B |
| Header bytes | 6,541,777 |
| Data start | 6,541,792 |
| Payload bytes | 2,483,352,832 (2.313 GiB) |
| Trailing padding | 0 |
| Overlapping tensors | 0 |

## Bytes by quantization type

| Type | Bytes | Share |
| --- | ---: | ---: |
| Q4_K | 1,529,118,720 | 61.57% |
| Q6_K | 952,761,600 | 38.37% |
| F32 | 1,472,512 | 0.06% |

## Metadata

| Key | Value |
| --- | --- |
| `gemma3.attention.head_count` | 8 |
| `gemma3.attention.head_count_kv` | 4 |
| `gemma3.attention.key_length` | 256 |
| `gemma3.attention.layer_norm_rms_epsilon` | 9.999999974752427e-07 |
| `gemma3.attention.sliding_window` | 1024 |
| `gemma3.attention.value_length` | 256 |
| `gemma3.block_count` | 34 |
| `gemma3.context_length` | 131072 |
| `gemma3.embedding_length` | 2560 |
| `gemma3.feed_forward_length` | 10240 |
| `gemma3.rope.freq_base` | 1000000.0 |
| `gemma3.rope.scaling.factor` | 8.0 |
| `gemma3.rope.scaling.type` | linear |
| `general.architecture` | gemma3 |
| `general.basename` | medgemma-1.5 |
| `general.file_type` | 15 |
| `general.finetune` | it |
| `general.license` | other |
| `general.license.link` | https://developers.google.com/health-ai-developer-foundations/terms |
| `general.license.name` | health-ai-developer-foundations |
| `general.name` | Medgemma 1.5 4b It |
| `general.quantization_version` | 2 |
| `general.sampling.top_k` | 64 |
| `general.sampling.top_p` | 0.949999988079071 |
| `general.size_label` | 4B |
| `general.source.url` | https://huggingface.co/google/medgemma-1.5-4b-it |
| `general.tags` | *array[8]* e.g. `['medical', 'radiology']` |
| `general.type` | model |
| `general.url` | https://huggingface.co/mradermacher/medgemma-1.5-4b-it-GGUF |
| `mradermacher.convert_type` | hf |
| `mradermacher.quantize_version` | 2 |
| `mradermacher.quantized_at` | 2026-01-14T07:02:22+01:00 |
| `mradermacher.quantized_by` | mradermacher |
| `mradermacher.quantized_on` | nico1 |
| `tokenizer.chat_template` | {{ bos_token }} {%- if messages[0]['role'] == 'system' -%}     {%- if messages[0]['content'] is string -%}         {%- set first_user_prefix = messages[0]['c... |
| `tokenizer.ggml.add_bos_token` | 1 |
| `tokenizer.ggml.add_eos_token` | 0 |
| `tokenizer.ggml.add_sep_token` | 0 |
| `tokenizer.ggml.add_space_prefix` | 0 |
| `tokenizer.ggml.bos_token_id` | 2 |
| `tokenizer.ggml.eos_token_id` | 1 |
| `tokenizer.ggml.model` | llama |
| `tokenizer.ggml.padding_token_id` | 0 |
| `tokenizer.ggml.pre` | default |
| `tokenizer.ggml.scores` | *array[262208]* e.g. `[-1000.0, -1000.0]` |
| `tokenizer.ggml.token_type` | *array[262208]* e.g. `[3, 3]` |
| `tokenizer.ggml.tokens` | *array[262208]* e.g. `['<pad>', '<eos>']` |
| `tokenizer.ggml.unknown_token_id` | 3 |

## Tensor table

Offsets are relative to the start of the tensor-data region.

| # | Tensor | Dims | Type | Offset | Bytes |
| ---: | --- | --- | --- | ---: | ---: |
| 0 | `output_norm.weight` | 2560 | F32 | 0 | 10,240 |
| 1 | `token_embd.weight` | 2560×262208 | Q6_K | 10,240 | 550,636,800 |
| 2 | `blk.0.attn_k.weight` | 2560×1024 | Q4_K | 550,647,040 | 1,474,560 |
| 3 | `blk.0.attn_k_norm.weight` | 256 | F32 | 552,121,600 | 1,024 |
| 4 | `blk.0.attn_norm.weight` | 2560 | F32 | 552,122,624 | 10,240 |
| 5 | `blk.0.attn_output.weight` | 2048×2560 | Q4_K | 552,132,864 | 2,949,120 |
| 6 | `blk.0.attn_q.weight` | 2560×2048 | Q4_K | 555,081,984 | 2,949,120 |
| 7 | `blk.0.attn_q_norm.weight` | 256 | F32 | 558,031,104 | 1,024 |
| 8 | `blk.0.attn_v.weight` | 2560×1024 | Q6_K | 558,032,128 | 2,150,400 |
| 9 | `blk.0.ffn_down.weight` | 10240×2560 | Q6_K | 560,182,528 | 21,504,000 |
| 10 | `blk.0.ffn_gate.weight` | 2560×10240 | Q4_K | 581,686,528 | 14,745,600 |
| 11 | `blk.0.ffn_norm.weight` | 2560 | F32 | 596,432,128 | 10,240 |
| 12 | `blk.0.ffn_up.weight` | 2560×10240 | Q4_K | 596,442,368 | 14,745,600 |
| 13 | `blk.0.post_attention_norm.weight` | 2560 | F32 | 611,187,968 | 10,240 |
| 14 | `blk.0.post_ffw_norm.weight` | 2560 | F32 | 611,198,208 | 10,240 |
| 15 | `blk.1.attn_k.weight` | 2560×1024 | Q4_K | 611,208,448 | 1,474,560 |
| 16 | `blk.1.attn_k_norm.weight` | 256 | F32 | 612,683,008 | 1,024 |
| 17 | `blk.1.attn_norm.weight` | 2560 | F32 | 612,684,032 | 10,240 |
| 18 | `blk.1.attn_output.weight` | 2048×2560 | Q4_K | 612,694,272 | 2,949,120 |
| 19 | `blk.1.attn_q.weight` | 2560×2048 | Q4_K | 615,643,392 | 2,949,120 |
| 20 | `blk.1.attn_q_norm.weight` | 256 | F32 | 618,592,512 | 1,024 |
| 21 | `blk.1.attn_v.weight` | 2560×1024 | Q6_K | 618,593,536 | 2,150,400 |
| 22 | `blk.1.ffn_down.weight` | 10240×2560 | Q6_K | 620,743,936 | 21,504,000 |
| 23 | `blk.1.ffn_gate.weight` | 2560×10240 | Q4_K | 642,247,936 | 14,745,600 |
| 24 | `blk.1.ffn_norm.weight` | 2560 | F32 | 656,993,536 | 10,240 |
| 25 | `blk.1.ffn_up.weight` | 2560×10240 | Q4_K | 657,003,776 | 14,745,600 |
| 26 | `blk.1.post_attention_norm.weight` | 2560 | F32 | 671,749,376 | 10,240 |
| 27 | `blk.1.post_ffw_norm.weight` | 2560 | F32 | 671,759,616 | 10,240 |
| 28 | `blk.2.attn_k.weight` | 2560×1024 | Q4_K | 671,769,856 | 1,474,560 |
| 29 | `blk.2.attn_k_norm.weight` | 256 | F32 | 673,244,416 | 1,024 |
| 30 | `blk.2.attn_norm.weight` | 2560 | F32 | 673,245,440 | 10,240 |
| 31 | `blk.2.attn_output.weight` | 2048×2560 | Q4_K | 673,255,680 | 2,949,120 |
| 32 | `blk.2.attn_q.weight` | 2560×2048 | Q4_K | 676,204,800 | 2,949,120 |
| 33 | `blk.2.attn_q_norm.weight` | 256 | F32 | 679,153,920 | 1,024 |
| 34 | `blk.2.attn_v.weight` | 2560×1024 | Q6_K | 679,154,944 | 2,150,400 |
| 35 | `blk.2.ffn_down.weight` | 10240×2560 | Q6_K | 681,305,344 | 21,504,000 |
| 36 | `blk.2.ffn_gate.weight` | 2560×10240 | Q4_K | 702,809,344 | 14,745,600 |
| 37 | `blk.2.ffn_norm.weight` | 2560 | F32 | 717,554,944 | 10,240 |
| 38 | `blk.2.ffn_up.weight` | 2560×10240 | Q4_K | 717,565,184 | 14,745,600 |
| 39 | `blk.2.post_attention_norm.weight` | 2560 | F32 | 732,310,784 | 10,240 |
| 40 | `blk.2.post_ffw_norm.weight` | 2560 | F32 | 732,321,024 | 10,240 |
| 41 | `blk.3.attn_k.weight` | 2560×1024 | Q4_K | 732,331,264 | 1,474,560 |
| 42 | `blk.3.attn_k_norm.weight` | 256 | F32 | 733,805,824 | 1,024 |
| 43 | `blk.3.attn_norm.weight` | 2560 | F32 | 733,806,848 | 10,240 |
| 44 | `blk.3.attn_output.weight` | 2048×2560 | Q4_K | 733,817,088 | 2,949,120 |
| 45 | `blk.3.attn_q.weight` | 2560×2048 | Q4_K | 736,766,208 | 2,949,120 |
| 46 | `blk.3.attn_q_norm.weight` | 256 | F32 | 739,715,328 | 1,024 |
| 47 | `blk.3.attn_v.weight` | 2560×1024 | Q6_K | 739,716,352 | 2,150,400 |
| 48 | `blk.3.ffn_down.weight` | 10240×2560 | Q6_K | 741,866,752 | 21,504,000 |
| 49 | `blk.3.ffn_gate.weight` | 2560×10240 | Q4_K | 763,370,752 | 14,745,600 |
| 50 | `blk.3.ffn_norm.weight` | 2560 | F32 | 778,116,352 | 10,240 |
| 51 | `blk.3.ffn_up.weight` | 2560×10240 | Q4_K | 778,126,592 | 14,745,600 |
| 52 | `blk.3.post_attention_norm.weight` | 2560 | F32 | 792,872,192 | 10,240 |
| 53 | `blk.3.post_ffw_norm.weight` | 2560 | F32 | 792,882,432 | 10,240 |
| 54 | `blk.4.attn_k.weight` | 2560×1024 | Q4_K | 792,892,672 | 1,474,560 |
| 55 | `blk.4.attn_k_norm.weight` | 256 | F32 | 794,367,232 | 1,024 |
| 56 | `blk.4.attn_norm.weight` | 2560 | F32 | 794,368,256 | 10,240 |
| 57 | `blk.4.attn_output.weight` | 2048×2560 | Q4_K | 794,378,496 | 2,949,120 |
| 58 | `blk.4.attn_q.weight` | 2560×2048 | Q4_K | 797,327,616 | 2,949,120 |
| 59 | `blk.4.attn_q_norm.weight` | 256 | F32 | 800,276,736 | 1,024 |
| 60 | `blk.4.attn_v.weight` | 2560×1024 | Q4_K | 800,277,760 | 1,474,560 |
| 61 | `blk.4.ffn_down.weight` | 10240×2560 | Q4_K | 801,752,320 | 14,745,600 |
| 62 | `blk.4.ffn_gate.weight` | 2560×10240 | Q4_K | 816,497,920 | 14,745,600 |
| 63 | `blk.4.ffn_norm.weight` | 2560 | F32 | 831,243,520 | 10,240 |
| 64 | `blk.4.ffn_up.weight` | 2560×10240 | Q4_K | 831,253,760 | 14,745,600 |
| 65 | `blk.4.post_attention_norm.weight` | 2560 | F32 | 845,999,360 | 10,240 |
| 66 | `blk.4.post_ffw_norm.weight` | 2560 | F32 | 846,009,600 | 10,240 |
| 67 | `blk.5.attn_k.weight` | 2560×1024 | Q4_K | 846,019,840 | 1,474,560 |
| 68 | `blk.5.attn_k_norm.weight` | 256 | F32 | 847,494,400 | 1,024 |
| 69 | `blk.5.attn_norm.weight` | 2560 | F32 | 847,495,424 | 10,240 |
| 70 | `blk.5.attn_output.weight` | 2048×2560 | Q4_K | 847,505,664 | 2,949,120 |
| 71 | `blk.5.attn_q.weight` | 2560×2048 | Q4_K | 850,454,784 | 2,949,120 |
| 72 | `blk.5.attn_q_norm.weight` | 256 | F32 | 853,403,904 | 1,024 |
| 73 | `blk.5.attn_v.weight` | 2560×1024 | Q4_K | 853,404,928 | 1,474,560 |
| 74 | `blk.5.ffn_down.weight` | 10240×2560 | Q4_K | 854,879,488 | 14,745,600 |
| 75 | `blk.5.ffn_gate.weight` | 2560×10240 | Q4_K | 869,625,088 | 14,745,600 |
| 76 | `blk.5.ffn_norm.weight` | 2560 | F32 | 884,370,688 | 10,240 |
| 77 | `blk.5.ffn_up.weight` | 2560×10240 | Q4_K | 884,380,928 | 14,745,600 |
| 78 | `blk.5.post_attention_norm.weight` | 2560 | F32 | 899,126,528 | 10,240 |
| 79 | `blk.5.post_ffw_norm.weight` | 2560 | F32 | 899,136,768 | 10,240 |
| 80 | `blk.6.attn_k.weight` | 2560×1024 | Q4_K | 899,147,008 | 1,474,560 |
| 81 | `blk.6.attn_k_norm.weight` | 256 | F32 | 900,621,568 | 1,024 |
| 82 | `blk.6.attn_norm.weight` | 2560 | F32 | 900,622,592 | 10,240 |
| 83 | `blk.6.attn_output.weight` | 2048×2560 | Q4_K | 900,632,832 | 2,949,120 |
| 84 | `blk.6.attn_q.weight` | 2560×2048 | Q4_K | 903,581,952 | 2,949,120 |
| 85 | `blk.6.attn_q_norm.weight` | 256 | F32 | 906,531,072 | 1,024 |
| 86 | `blk.6.attn_v.weight` | 2560×1024 | Q6_K | 906,532,096 | 2,150,400 |
| 87 | `blk.6.ffn_down.weight` | 10240×2560 | Q6_K | 908,682,496 | 21,504,000 |
| 88 | `blk.6.ffn_gate.weight` | 2560×10240 | Q4_K | 930,186,496 | 14,745,600 |
| 89 | `blk.6.ffn_norm.weight` | 2560 | F32 | 944,932,096 | 10,240 |
| 90 | `blk.6.ffn_up.weight` | 2560×10240 | Q4_K | 944,942,336 | 14,745,600 |
| 91 | `blk.6.post_attention_norm.weight` | 2560 | F32 | 959,687,936 | 10,240 |
| 92 | `blk.6.post_ffw_norm.weight` | 2560 | F32 | 959,698,176 | 10,240 |
| 93 | `blk.7.attn_k.weight` | 2560×1024 | Q4_K | 959,708,416 | 1,474,560 |
| 94 | `blk.7.attn_k_norm.weight` | 256 | F32 | 961,182,976 | 1,024 |
| 95 | `blk.7.attn_norm.weight` | 2560 | F32 | 961,184,000 | 10,240 |
| 96 | `blk.7.attn_output.weight` | 2048×2560 | Q4_K | 961,194,240 | 2,949,120 |
| 97 | `blk.7.attn_q.weight` | 2560×2048 | Q4_K | 964,143,360 | 2,949,120 |
| 98 | `blk.7.attn_q_norm.weight` | 256 | F32 | 967,092,480 | 1,024 |
| 99 | `blk.7.attn_v.weight` | 2560×1024 | Q4_K | 967,093,504 | 1,474,560 |
| 100 | `blk.7.ffn_down.weight` | 10240×2560 | Q4_K | 968,568,064 | 14,745,600 |
| 101 | `blk.7.ffn_gate.weight` | 2560×10240 | Q4_K | 983,313,664 | 14,745,600 |
| 102 | `blk.7.ffn_norm.weight` | 2560 | F32 | 998,059,264 | 10,240 |
| 103 | `blk.7.ffn_up.weight` | 2560×10240 | Q4_K | 998,069,504 | 14,745,600 |
| 104 | `blk.7.post_attention_norm.weight` | 2560 | F32 | 1,012,815,104 | 10,240 |
| 105 | `blk.7.post_ffw_norm.weight` | 2560 | F32 | 1,012,825,344 | 10,240 |
| 106 | `blk.8.attn_k.weight` | 2560×1024 | Q4_K | 1,012,835,584 | 1,474,560 |
| 107 | `blk.8.attn_k_norm.weight` | 256 | F32 | 1,014,310,144 | 1,024 |
| 108 | `blk.8.attn_norm.weight` | 2560 | F32 | 1,014,311,168 | 10,240 |
| 109 | `blk.8.attn_output.weight` | 2048×2560 | Q4_K | 1,014,321,408 | 2,949,120 |
| 110 | `blk.8.attn_q.weight` | 2560×2048 | Q4_K | 1,017,270,528 | 2,949,120 |
| 111 | `blk.8.attn_q_norm.weight` | 256 | F32 | 1,020,219,648 | 1,024 |
| 112 | `blk.8.attn_v.weight` | 2560×1024 | Q4_K | 1,020,220,672 | 1,474,560 |
| 113 | `blk.8.ffn_down.weight` | 10240×2560 | Q4_K | 1,021,695,232 | 14,745,600 |
| 114 | `blk.8.ffn_gate.weight` | 2560×10240 | Q4_K | 1,036,440,832 | 14,745,600 |
| 115 | `blk.8.ffn_norm.weight` | 2560 | F32 | 1,051,186,432 | 10,240 |
| 116 | `blk.8.ffn_up.weight` | 2560×10240 | Q4_K | 1,051,196,672 | 14,745,600 |
| 117 | `blk.8.post_attention_norm.weight` | 2560 | F32 | 1,065,942,272 | 10,240 |
| 118 | `blk.8.post_ffw_norm.weight` | 2560 | F32 | 1,065,952,512 | 10,240 |
| 119 | `blk.9.attn_k.weight` | 2560×1024 | Q4_K | 1,065,962,752 | 1,474,560 |
| 120 | `blk.9.attn_k_norm.weight` | 256 | F32 | 1,067,437,312 | 1,024 |
| 121 | `blk.9.attn_norm.weight` | 2560 | F32 | 1,067,438,336 | 10,240 |
| 122 | `blk.9.attn_output.weight` | 2048×2560 | Q4_K | 1,067,448,576 | 2,949,120 |
| 123 | `blk.9.attn_q.weight` | 2560×2048 | Q4_K | 1,070,397,696 | 2,949,120 |
| 124 | `blk.9.attn_q_norm.weight` | 256 | F32 | 1,073,346,816 | 1,024 |
| 125 | `blk.9.attn_v.weight` | 2560×1024 | Q6_K | 1,073,347,840 | 2,150,400 |
| 126 | `blk.9.ffn_down.weight` | 10240×2560 | Q6_K | 1,075,498,240 | 21,504,000 |
| 127 | `blk.9.ffn_gate.weight` | 2560×10240 | Q4_K | 1,097,002,240 | 14,745,600 |
| 128 | `blk.9.ffn_norm.weight` | 2560 | F32 | 1,111,747,840 | 10,240 |
| 129 | `blk.9.ffn_up.weight` | 2560×10240 | Q4_K | 1,111,758,080 | 14,745,600 |
| 130 | `blk.9.post_attention_norm.weight` | 2560 | F32 | 1,126,503,680 | 10,240 |
| 131 | `blk.9.post_ffw_norm.weight` | 2560 | F32 | 1,126,513,920 | 10,240 |
| 132 | `blk.10.attn_k.weight` | 2560×1024 | Q4_K | 1,126,524,160 | 1,474,560 |
| 133 | `blk.10.attn_k_norm.weight` | 256 | F32 | 1,127,998,720 | 1,024 |
| 134 | `blk.10.attn_norm.weight` | 2560 | F32 | 1,127,999,744 | 10,240 |
| 135 | `blk.10.attn_output.weight` | 2048×2560 | Q4_K | 1,128,009,984 | 2,949,120 |
| 136 | `blk.10.attn_q.weight` | 2560×2048 | Q4_K | 1,130,959,104 | 2,949,120 |
| 137 | `blk.10.attn_q_norm.weight` | 256 | F32 | 1,133,908,224 | 1,024 |
| 138 | `blk.10.attn_v.weight` | 2560×1024 | Q4_K | 1,133,909,248 | 1,474,560 |
| 139 | `blk.10.ffn_down.weight` | 10240×2560 | Q4_K | 1,135,383,808 | 14,745,600 |
| 140 | `blk.10.ffn_gate.weight` | 2560×10240 | Q4_K | 1,150,129,408 | 14,745,600 |
| 141 | `blk.10.ffn_norm.weight` | 2560 | F32 | 1,164,875,008 | 10,240 |
| 142 | `blk.10.ffn_up.weight` | 2560×10240 | Q4_K | 1,164,885,248 | 14,745,600 |
| 143 | `blk.10.post_attention_norm.weight` | 2560 | F32 | 1,179,630,848 | 10,240 |
| 144 | `blk.10.post_ffw_norm.weight` | 2560 | F32 | 1,179,641,088 | 10,240 |
| 145 | `blk.11.attn_k.weight` | 2560×1024 | Q4_K | 1,179,651,328 | 1,474,560 |
| 146 | `blk.11.attn_k_norm.weight` | 256 | F32 | 1,181,125,888 | 1,024 |
| 147 | `blk.11.attn_norm.weight` | 2560 | F32 | 1,181,126,912 | 10,240 |
| 148 | `blk.11.attn_output.weight` | 2048×2560 | Q4_K | 1,181,137,152 | 2,949,120 |
| 149 | `blk.11.attn_q.weight` | 2560×2048 | Q4_K | 1,184,086,272 | 2,949,120 |
| 150 | `blk.11.attn_q_norm.weight` | 256 | F32 | 1,187,035,392 | 1,024 |
| 151 | `blk.11.attn_v.weight` | 2560×1024 | Q4_K | 1,187,036,416 | 1,474,560 |
| 152 | `blk.11.ffn_down.weight` | 10240×2560 | Q4_K | 1,188,510,976 | 14,745,600 |
| 153 | `blk.11.ffn_gate.weight` | 2560×10240 | Q4_K | 1,203,256,576 | 14,745,600 |
| 154 | `blk.11.ffn_norm.weight` | 2560 | F32 | 1,218,002,176 | 10,240 |
| 155 | `blk.11.ffn_up.weight` | 2560×10240 | Q4_K | 1,218,012,416 | 14,745,600 |
| 156 | `blk.11.post_attention_norm.weight` | 2560 | F32 | 1,232,758,016 | 10,240 |
| 157 | `blk.11.post_ffw_norm.weight` | 2560 | F32 | 1,232,768,256 | 10,240 |
| 158 | `blk.12.attn_k.weight` | 2560×1024 | Q4_K | 1,232,778,496 | 1,474,560 |
| 159 | `blk.12.attn_k_norm.weight` | 256 | F32 | 1,234,253,056 | 1,024 |
| 160 | `blk.12.attn_norm.weight` | 2560 | F32 | 1,234,254,080 | 10,240 |
| 161 | `blk.12.attn_output.weight` | 2048×2560 | Q4_K | 1,234,264,320 | 2,949,120 |
| 162 | `blk.12.attn_q.weight` | 2560×2048 | Q4_K | 1,237,213,440 | 2,949,120 |
| 163 | `blk.12.attn_q_norm.weight` | 256 | F32 | 1,240,162,560 | 1,024 |
| 164 | `blk.12.attn_v.weight` | 2560×1024 | Q6_K | 1,240,163,584 | 2,150,400 |
| 165 | `blk.12.ffn_down.weight` | 10240×2560 | Q6_K | 1,242,313,984 | 21,504,000 |
| 166 | `blk.12.ffn_gate.weight` | 2560×10240 | Q4_K | 1,263,817,984 | 14,745,600 |
| 167 | `blk.12.ffn_norm.weight` | 2560 | F32 | 1,278,563,584 | 10,240 |
| 168 | `blk.12.ffn_up.weight` | 2560×10240 | Q4_K | 1,278,573,824 | 14,745,600 |
| 169 | `blk.12.post_attention_norm.weight` | 2560 | F32 | 1,293,319,424 | 10,240 |
| 170 | `blk.12.post_ffw_norm.weight` | 2560 | F32 | 1,293,329,664 | 10,240 |
| 171 | `blk.13.attn_k.weight` | 2560×1024 | Q4_K | 1,293,339,904 | 1,474,560 |
| 172 | `blk.13.attn_k_norm.weight` | 256 | F32 | 1,294,814,464 | 1,024 |
| 173 | `blk.13.attn_norm.weight` | 2560 | F32 | 1,294,815,488 | 10,240 |
| 174 | `blk.13.attn_output.weight` | 2048×2560 | Q4_K | 1,294,825,728 | 2,949,120 |
| 175 | `blk.13.attn_q.weight` | 2560×2048 | Q4_K | 1,297,774,848 | 2,949,120 |
| 176 | `blk.13.attn_q_norm.weight` | 256 | F32 | 1,300,723,968 | 1,024 |
| 177 | `blk.13.attn_v.weight` | 2560×1024 | Q4_K | 1,300,724,992 | 1,474,560 |
| 178 | `blk.13.ffn_down.weight` | 10240×2560 | Q4_K | 1,302,199,552 | 14,745,600 |
| 179 | `blk.13.ffn_gate.weight` | 2560×10240 | Q4_K | 1,316,945,152 | 14,745,600 |
| 180 | `blk.13.ffn_norm.weight` | 2560 | F32 | 1,331,690,752 | 10,240 |
| 181 | `blk.13.ffn_up.weight` | 2560×10240 | Q4_K | 1,331,700,992 | 14,745,600 |
| 182 | `blk.13.post_attention_norm.weight` | 2560 | F32 | 1,346,446,592 | 10,240 |
| 183 | `blk.13.post_ffw_norm.weight` | 2560 | F32 | 1,346,456,832 | 10,240 |
| 184 | `blk.14.attn_k.weight` | 2560×1024 | Q4_K | 1,346,467,072 | 1,474,560 |
| 185 | `blk.14.attn_k_norm.weight` | 256 | F32 | 1,347,941,632 | 1,024 |
| 186 | `blk.14.attn_norm.weight` | 2560 | F32 | 1,347,942,656 | 10,240 |
| 187 | `blk.14.attn_output.weight` | 2048×2560 | Q4_K | 1,347,952,896 | 2,949,120 |
| 188 | `blk.14.attn_q.weight` | 2560×2048 | Q4_K | 1,350,902,016 | 2,949,120 |
| 189 | `blk.14.attn_q_norm.weight` | 256 | F32 | 1,353,851,136 | 1,024 |
| 190 | `blk.14.attn_v.weight` | 2560×1024 | Q4_K | 1,353,852,160 | 1,474,560 |
| 191 | `blk.14.ffn_down.weight` | 10240×2560 | Q4_K | 1,355,326,720 | 14,745,600 |
| 192 | `blk.14.ffn_gate.weight` | 2560×10240 | Q4_K | 1,370,072,320 | 14,745,600 |
| 193 | `blk.14.ffn_norm.weight` | 2560 | F32 | 1,384,817,920 | 10,240 |
| 194 | `blk.14.ffn_up.weight` | 2560×10240 | Q4_K | 1,384,828,160 | 14,745,600 |
| 195 | `blk.14.post_attention_norm.weight` | 2560 | F32 | 1,399,573,760 | 10,240 |
| 196 | `blk.14.post_ffw_norm.weight` | 2560 | F32 | 1,399,584,000 | 10,240 |
| 197 | `blk.15.attn_k.weight` | 2560×1024 | Q4_K | 1,399,594,240 | 1,474,560 |
| 198 | `blk.15.attn_k_norm.weight` | 256 | F32 | 1,401,068,800 | 1,024 |
| 199 | `blk.15.attn_norm.weight` | 2560 | F32 | 1,401,069,824 | 10,240 |
| 200 | `blk.15.attn_output.weight` | 2048×2560 | Q4_K | 1,401,080,064 | 2,949,120 |
| 201 | `blk.15.attn_q.weight` | 2560×2048 | Q4_K | 1,404,029,184 | 2,949,120 |
| 202 | `blk.15.attn_q_norm.weight` | 256 | F32 | 1,406,978,304 | 1,024 |
| 203 | `blk.15.attn_v.weight` | 2560×1024 | Q6_K | 1,406,979,328 | 2,150,400 |
| 204 | `blk.15.ffn_down.weight` | 10240×2560 | Q6_K | 1,409,129,728 | 21,504,000 |
| 205 | `blk.15.ffn_gate.weight` | 2560×10240 | Q4_K | 1,430,633,728 | 14,745,600 |
| 206 | `blk.15.ffn_norm.weight` | 2560 | F32 | 1,445,379,328 | 10,240 |
| 207 | `blk.15.ffn_up.weight` | 2560×10240 | Q4_K | 1,445,389,568 | 14,745,600 |
| 208 | `blk.15.post_attention_norm.weight` | 2560 | F32 | 1,460,135,168 | 10,240 |
| 209 | `blk.15.post_ffw_norm.weight` | 2560 | F32 | 1,460,145,408 | 10,240 |
| 210 | `blk.16.attn_k.weight` | 2560×1024 | Q4_K | 1,460,155,648 | 1,474,560 |
| 211 | `blk.16.attn_k_norm.weight` | 256 | F32 | 1,461,630,208 | 1,024 |
| 212 | `blk.16.attn_norm.weight` | 2560 | F32 | 1,461,631,232 | 10,240 |
| 213 | `blk.16.attn_output.weight` | 2048×2560 | Q4_K | 1,461,641,472 | 2,949,120 |
| 214 | `blk.16.attn_q.weight` | 2560×2048 | Q4_K | 1,464,590,592 | 2,949,120 |
| 215 | `blk.16.attn_q_norm.weight` | 256 | F32 | 1,467,539,712 | 1,024 |
| 216 | `blk.16.attn_v.weight` | 2560×1024 | Q4_K | 1,467,540,736 | 1,474,560 |
| 217 | `blk.16.ffn_down.weight` | 10240×2560 | Q4_K | 1,469,015,296 | 14,745,600 |
| 218 | `blk.16.ffn_gate.weight` | 2560×10240 | Q4_K | 1,483,760,896 | 14,745,600 |
| 219 | `blk.16.ffn_norm.weight` | 2560 | F32 | 1,498,506,496 | 10,240 |
| 220 | `blk.16.ffn_up.weight` | 2560×10240 | Q4_K | 1,498,516,736 | 14,745,600 |
| 221 | `blk.16.post_attention_norm.weight` | 2560 | F32 | 1,513,262,336 | 10,240 |
| 222 | `blk.16.post_ffw_norm.weight` | 2560 | F32 | 1,513,272,576 | 10,240 |
| 223 | `blk.17.attn_k.weight` | 2560×1024 | Q4_K | 1,513,282,816 | 1,474,560 |
| 224 | `blk.17.attn_k_norm.weight` | 256 | F32 | 1,514,757,376 | 1,024 |
| 225 | `blk.17.attn_norm.weight` | 2560 | F32 | 1,514,758,400 | 10,240 |
| 226 | `blk.17.attn_output.weight` | 2048×2560 | Q4_K | 1,514,768,640 | 2,949,120 |
| 227 | `blk.17.attn_q.weight` | 2560×2048 | Q4_K | 1,517,717,760 | 2,949,120 |
| 228 | `blk.17.attn_q_norm.weight` | 256 | F32 | 1,520,666,880 | 1,024 |
| 229 | `blk.17.attn_v.weight` | 2560×1024 | Q4_K | 1,520,667,904 | 1,474,560 |
| 230 | `blk.17.ffn_down.weight` | 10240×2560 | Q4_K | 1,522,142,464 | 14,745,600 |
| 231 | `blk.17.ffn_gate.weight` | 2560×10240 | Q4_K | 1,536,888,064 | 14,745,600 |
| 232 | `blk.17.ffn_norm.weight` | 2560 | F32 | 1,551,633,664 | 10,240 |
| 233 | `blk.17.ffn_up.weight` | 2560×10240 | Q4_K | 1,551,643,904 | 14,745,600 |
| 234 | `blk.17.post_attention_norm.weight` | 2560 | F32 | 1,566,389,504 | 10,240 |
| 235 | `blk.17.post_ffw_norm.weight` | 2560 | F32 | 1,566,399,744 | 10,240 |
| 236 | `blk.18.attn_k.weight` | 2560×1024 | Q4_K | 1,566,409,984 | 1,474,560 |
| 237 | `blk.18.attn_k_norm.weight` | 256 | F32 | 1,567,884,544 | 1,024 |
| 238 | `blk.18.attn_norm.weight` | 2560 | F32 | 1,567,885,568 | 10,240 |
| 239 | `blk.18.attn_output.weight` | 2048×2560 | Q4_K | 1,567,895,808 | 2,949,120 |
| 240 | `blk.18.attn_q.weight` | 2560×2048 | Q4_K | 1,570,844,928 | 2,949,120 |
| 241 | `blk.18.attn_q_norm.weight` | 256 | F32 | 1,573,794,048 | 1,024 |
| 242 | `blk.18.attn_v.weight` | 2560×1024 | Q6_K | 1,573,795,072 | 2,150,400 |
| 243 | `blk.18.ffn_down.weight` | 10240×2560 | Q6_K | 1,575,945,472 | 21,504,000 |
| 244 | `blk.18.ffn_gate.weight` | 2560×10240 | Q4_K | 1,597,449,472 | 14,745,600 |
| 245 | `blk.18.ffn_norm.weight` | 2560 | F32 | 1,612,195,072 | 10,240 |
| 246 | `blk.18.ffn_up.weight` | 2560×10240 | Q4_K | 1,612,205,312 | 14,745,600 |
| 247 | `blk.18.post_attention_norm.weight` | 2560 | F32 | 1,626,950,912 | 10,240 |
| 248 | `blk.18.post_ffw_norm.weight` | 2560 | F32 | 1,626,961,152 | 10,240 |
| 249 | `blk.19.attn_k.weight` | 2560×1024 | Q4_K | 1,626,971,392 | 1,474,560 |
| 250 | `blk.19.attn_k_norm.weight` | 256 | F32 | 1,628,445,952 | 1,024 |
| 251 | `blk.19.attn_norm.weight` | 2560 | F32 | 1,628,446,976 | 10,240 |
| 252 | `blk.19.attn_output.weight` | 2048×2560 | Q4_K | 1,628,457,216 | 2,949,120 |
| 253 | `blk.19.attn_q.weight` | 2560×2048 | Q4_K | 1,631,406,336 | 2,949,120 |
| 254 | `blk.19.attn_q_norm.weight` | 256 | F32 | 1,634,355,456 | 1,024 |
| 255 | `blk.19.attn_v.weight` | 2560×1024 | Q4_K | 1,634,356,480 | 1,474,560 |
| 256 | `blk.19.ffn_down.weight` | 10240×2560 | Q4_K | 1,635,831,040 | 14,745,600 |
| 257 | `blk.19.ffn_gate.weight` | 2560×10240 | Q4_K | 1,650,576,640 | 14,745,600 |
| 258 | `blk.19.ffn_norm.weight` | 2560 | F32 | 1,665,322,240 | 10,240 |
| 259 | `blk.19.ffn_up.weight` | 2560×10240 | Q4_K | 1,665,332,480 | 14,745,600 |
| 260 | `blk.19.post_attention_norm.weight` | 2560 | F32 | 1,680,078,080 | 10,240 |
| 261 | `blk.19.post_ffw_norm.weight` | 2560 | F32 | 1,680,088,320 | 10,240 |
| 262 | `blk.20.attn_k.weight` | 2560×1024 | Q4_K | 1,680,098,560 | 1,474,560 |
| 263 | `blk.20.attn_k_norm.weight` | 256 | F32 | 1,681,573,120 | 1,024 |
| 264 | `blk.20.attn_norm.weight` | 2560 | F32 | 1,681,574,144 | 10,240 |
| 265 | `blk.20.attn_output.weight` | 2048×2560 | Q4_K | 1,681,584,384 | 2,949,120 |
| 266 | `blk.20.attn_q.weight` | 2560×2048 | Q4_K | 1,684,533,504 | 2,949,120 |
| 267 | `blk.20.attn_q_norm.weight` | 256 | F32 | 1,687,482,624 | 1,024 |
| 268 | `blk.20.attn_v.weight` | 2560×1024 | Q4_K | 1,687,483,648 | 1,474,560 |
| 269 | `blk.20.ffn_down.weight` | 10240×2560 | Q4_K | 1,688,958,208 | 14,745,600 |
| 270 | `blk.20.ffn_gate.weight` | 2560×10240 | Q4_K | 1,703,703,808 | 14,745,600 |
| 271 | `blk.20.ffn_norm.weight` | 2560 | F32 | 1,718,449,408 | 10,240 |
| 272 | `blk.20.ffn_up.weight` | 2560×10240 | Q4_K | 1,718,459,648 | 14,745,600 |
| 273 | `blk.20.post_attention_norm.weight` | 2560 | F32 | 1,733,205,248 | 10,240 |
| 274 | `blk.20.post_ffw_norm.weight` | 2560 | F32 | 1,733,215,488 | 10,240 |
| 275 | `blk.21.attn_k.weight` | 2560×1024 | Q4_K | 1,733,225,728 | 1,474,560 |
| 276 | `blk.21.attn_k_norm.weight` | 256 | F32 | 1,734,700,288 | 1,024 |
| 277 | `blk.21.attn_norm.weight` | 2560 | F32 | 1,734,701,312 | 10,240 |
| 278 | `blk.21.attn_output.weight` | 2048×2560 | Q4_K | 1,734,711,552 | 2,949,120 |
| 279 | `blk.21.attn_q.weight` | 2560×2048 | Q4_K | 1,737,660,672 | 2,949,120 |
| 280 | `blk.21.attn_q_norm.weight` | 256 | F32 | 1,740,609,792 | 1,024 |
| 281 | `blk.21.attn_v.weight` | 2560×1024 | Q6_K | 1,740,610,816 | 2,150,400 |
| 282 | `blk.21.ffn_down.weight` | 10240×2560 | Q6_K | 1,742,761,216 | 21,504,000 |
| 283 | `blk.21.ffn_gate.weight` | 2560×10240 | Q4_K | 1,764,265,216 | 14,745,600 |
| 284 | `blk.21.ffn_norm.weight` | 2560 | F32 | 1,779,010,816 | 10,240 |
| 285 | `blk.21.ffn_up.weight` | 2560×10240 | Q4_K | 1,779,021,056 | 14,745,600 |
| 286 | `blk.21.post_attention_norm.weight` | 2560 | F32 | 1,793,766,656 | 10,240 |
| 287 | `blk.21.post_ffw_norm.weight` | 2560 | F32 | 1,793,776,896 | 10,240 |
| 288 | `blk.22.attn_k.weight` | 2560×1024 | Q4_K | 1,793,787,136 | 1,474,560 |
| 289 | `blk.22.attn_k_norm.weight` | 256 | F32 | 1,795,261,696 | 1,024 |
| 290 | `blk.22.attn_norm.weight` | 2560 | F32 | 1,795,262,720 | 10,240 |
| 291 | `blk.22.attn_output.weight` | 2048×2560 | Q4_K | 1,795,272,960 | 2,949,120 |
| 292 | `blk.22.attn_q.weight` | 2560×2048 | Q4_K | 1,798,222,080 | 2,949,120 |
| 293 | `blk.22.attn_q_norm.weight` | 256 | F32 | 1,801,171,200 | 1,024 |
| 294 | `blk.22.attn_v.weight` | 2560×1024 | Q4_K | 1,801,172,224 | 1,474,560 |
| 295 | `blk.22.ffn_down.weight` | 10240×2560 | Q4_K | 1,802,646,784 | 14,745,600 |
| 296 | `blk.22.ffn_gate.weight` | 2560×10240 | Q4_K | 1,817,392,384 | 14,745,600 |
| 297 | `blk.22.ffn_norm.weight` | 2560 | F32 | 1,832,137,984 | 10,240 |
| 298 | `blk.22.ffn_up.weight` | 2560×10240 | Q4_K | 1,832,148,224 | 14,745,600 |
| 299 | `blk.22.post_attention_norm.weight` | 2560 | F32 | 1,846,893,824 | 10,240 |
| 300 | `blk.22.post_ffw_norm.weight` | 2560 | F32 | 1,846,904,064 | 10,240 |
| 301 | `blk.23.attn_k.weight` | 2560×1024 | Q4_K | 1,846,914,304 | 1,474,560 |
| 302 | `blk.23.attn_k_norm.weight` | 256 | F32 | 1,848,388,864 | 1,024 |
| 303 | `blk.23.attn_norm.weight` | 2560 | F32 | 1,848,389,888 | 10,240 |
| 304 | `blk.23.attn_output.weight` | 2048×2560 | Q4_K | 1,848,400,128 | 2,949,120 |
| 305 | `blk.23.attn_q.weight` | 2560×2048 | Q4_K | 1,851,349,248 | 2,949,120 |
| 306 | `blk.23.attn_q_norm.weight` | 256 | F32 | 1,854,298,368 | 1,024 |
| 307 | `blk.23.attn_v.weight` | 2560×1024 | Q4_K | 1,854,299,392 | 1,474,560 |
| 308 | `blk.23.ffn_down.weight` | 10240×2560 | Q4_K | 1,855,773,952 | 14,745,600 |
| 309 | `blk.23.ffn_gate.weight` | 2560×10240 | Q4_K | 1,870,519,552 | 14,745,600 |
| 310 | `blk.23.ffn_norm.weight` | 2560 | F32 | 1,885,265,152 | 10,240 |
| 311 | `blk.23.ffn_up.weight` | 2560×10240 | Q4_K | 1,885,275,392 | 14,745,600 |
| 312 | `blk.23.post_attention_norm.weight` | 2560 | F32 | 1,900,020,992 | 10,240 |
| 313 | `blk.23.post_ffw_norm.weight` | 2560 | F32 | 1,900,031,232 | 10,240 |
| 314 | `blk.24.attn_k.weight` | 2560×1024 | Q4_K | 1,900,041,472 | 1,474,560 |
| 315 | `blk.24.attn_k_norm.weight` | 256 | F32 | 1,901,516,032 | 1,024 |
| 316 | `blk.24.attn_norm.weight` | 2560 | F32 | 1,901,517,056 | 10,240 |
| 317 | `blk.24.attn_output.weight` | 2048×2560 | Q4_K | 1,901,527,296 | 2,949,120 |
| 318 | `blk.24.attn_q.weight` | 2560×2048 | Q4_K | 1,904,476,416 | 2,949,120 |
| 319 | `blk.24.attn_q_norm.weight` | 256 | F32 | 1,907,425,536 | 1,024 |
| 320 | `blk.24.attn_v.weight` | 2560×1024 | Q6_K | 1,907,426,560 | 2,150,400 |
| 321 | `blk.24.ffn_down.weight` | 10240×2560 | Q6_K | 1,909,576,960 | 21,504,000 |
| 322 | `blk.24.ffn_gate.weight` | 2560×10240 | Q4_K | 1,931,080,960 | 14,745,600 |
| 323 | `blk.24.ffn_norm.weight` | 2560 | F32 | 1,945,826,560 | 10,240 |
| 324 | `blk.24.ffn_up.weight` | 2560×10240 | Q4_K | 1,945,836,800 | 14,745,600 |
| 325 | `blk.24.post_attention_norm.weight` | 2560 | F32 | 1,960,582,400 | 10,240 |
| 326 | `blk.24.post_ffw_norm.weight` | 2560 | F32 | 1,960,592,640 | 10,240 |
| 327 | `blk.25.attn_k.weight` | 2560×1024 | Q4_K | 1,960,602,880 | 1,474,560 |
| 328 | `blk.25.attn_k_norm.weight` | 256 | F32 | 1,962,077,440 | 1,024 |
| 329 | `blk.25.attn_norm.weight` | 2560 | F32 | 1,962,078,464 | 10,240 |
| 330 | `blk.25.attn_output.weight` | 2048×2560 | Q4_K | 1,962,088,704 | 2,949,120 |
| 331 | `blk.25.attn_q.weight` | 2560×2048 | Q4_K | 1,965,037,824 | 2,949,120 |
| 332 | `blk.25.attn_q_norm.weight` | 256 | F32 | 1,967,986,944 | 1,024 |
| 333 | `blk.25.attn_v.weight` | 2560×1024 | Q4_K | 1,967,987,968 | 1,474,560 |
| 334 | `blk.25.ffn_down.weight` | 10240×2560 | Q4_K | 1,969,462,528 | 14,745,600 |
| 335 | `blk.25.ffn_gate.weight` | 2560×10240 | Q4_K | 1,984,208,128 | 14,745,600 |
| 336 | `blk.25.ffn_norm.weight` | 2560 | F32 | 1,998,953,728 | 10,240 |
| 337 | `blk.25.ffn_up.weight` | 2560×10240 | Q4_K | 1,998,963,968 | 14,745,600 |
| 338 | `blk.25.post_attention_norm.weight` | 2560 | F32 | 2,013,709,568 | 10,240 |
| 339 | `blk.25.post_ffw_norm.weight` | 2560 | F32 | 2,013,719,808 | 10,240 |
| 340 | `blk.26.attn_k.weight` | 2560×1024 | Q4_K | 2,013,730,048 | 1,474,560 |
| 341 | `blk.26.attn_k_norm.weight` | 256 | F32 | 2,015,204,608 | 1,024 |
| 342 | `blk.26.attn_norm.weight` | 2560 | F32 | 2,015,205,632 | 10,240 |
| 343 | `blk.26.attn_output.weight` | 2048×2560 | Q4_K | 2,015,215,872 | 2,949,120 |
| 344 | `blk.26.attn_q.weight` | 2560×2048 | Q4_K | 2,018,164,992 | 2,949,120 |
| 345 | `blk.26.attn_q_norm.weight` | 256 | F32 | 2,021,114,112 | 1,024 |
| 346 | `blk.26.attn_v.weight` | 2560×1024 | Q4_K | 2,021,115,136 | 1,474,560 |
| 347 | `blk.26.ffn_down.weight` | 10240×2560 | Q4_K | 2,022,589,696 | 14,745,600 |
| 348 | `blk.26.ffn_gate.weight` | 2560×10240 | Q4_K | 2,037,335,296 | 14,745,600 |
| 349 | `blk.26.ffn_norm.weight` | 2560 | F32 | 2,052,080,896 | 10,240 |
| 350 | `blk.26.ffn_up.weight` | 2560×10240 | Q4_K | 2,052,091,136 | 14,745,600 |
| 351 | `blk.26.post_attention_norm.weight` | 2560 | F32 | 2,066,836,736 | 10,240 |
| 352 | `blk.26.post_ffw_norm.weight` | 2560 | F32 | 2,066,846,976 | 10,240 |
| 353 | `blk.27.attn_k.weight` | 2560×1024 | Q4_K | 2,066,857,216 | 1,474,560 |
| 354 | `blk.27.attn_k_norm.weight` | 256 | F32 | 2,068,331,776 | 1,024 |
| 355 | `blk.27.attn_norm.weight` | 2560 | F32 | 2,068,332,800 | 10,240 |
| 356 | `blk.27.attn_output.weight` | 2048×2560 | Q4_K | 2,068,343,040 | 2,949,120 |
| 357 | `blk.27.attn_q.weight` | 2560×2048 | Q4_K | 2,071,292,160 | 2,949,120 |
| 358 | `blk.27.attn_q_norm.weight` | 256 | F32 | 2,074,241,280 | 1,024 |
| 359 | `blk.27.attn_v.weight` | 2560×1024 | Q6_K | 2,074,242,304 | 2,150,400 |
| 360 | `blk.27.ffn_down.weight` | 10240×2560 | Q6_K | 2,076,392,704 | 21,504,000 |
| 361 | `blk.27.ffn_gate.weight` | 2560×10240 | Q4_K | 2,097,896,704 | 14,745,600 |
| 362 | `blk.27.ffn_norm.weight` | 2560 | F32 | 2,112,642,304 | 10,240 |
| 363 | `blk.27.ffn_up.weight` | 2560×10240 | Q4_K | 2,112,652,544 | 14,745,600 |
| 364 | `blk.27.post_attention_norm.weight` | 2560 | F32 | 2,127,398,144 | 10,240 |
| 365 | `blk.27.post_ffw_norm.weight` | 2560 | F32 | 2,127,408,384 | 10,240 |
| 366 | `blk.28.attn_k.weight` | 2560×1024 | Q4_K | 2,127,418,624 | 1,474,560 |
| 367 | `blk.28.attn_k_norm.weight` | 256 | F32 | 2,128,893,184 | 1,024 |
| 368 | `blk.28.attn_norm.weight` | 2560 | F32 | 2,128,894,208 | 10,240 |
| 369 | `blk.28.attn_output.weight` | 2048×2560 | Q4_K | 2,128,904,448 | 2,949,120 |
| 370 | `blk.28.attn_q.weight` | 2560×2048 | Q4_K | 2,131,853,568 | 2,949,120 |
| 371 | `blk.28.attn_q_norm.weight` | 256 | F32 | 2,134,802,688 | 1,024 |
| 372 | `blk.28.attn_v.weight` | 2560×1024 | Q4_K | 2,134,803,712 | 1,474,560 |
| 373 | `blk.28.ffn_down.weight` | 10240×2560 | Q4_K | 2,136,278,272 | 14,745,600 |
| 374 | `blk.28.ffn_gate.weight` | 2560×10240 | Q4_K | 2,151,023,872 | 14,745,600 |
| 375 | `blk.28.ffn_norm.weight` | 2560 | F32 | 2,165,769,472 | 10,240 |
| 376 | `blk.28.ffn_up.weight` | 2560×10240 | Q4_K | 2,165,779,712 | 14,745,600 |
| 377 | `blk.28.post_attention_norm.weight` | 2560 | F32 | 2,180,525,312 | 10,240 |
| 378 | `blk.28.post_ffw_norm.weight` | 2560 | F32 | 2,180,535,552 | 10,240 |
| 379 | `blk.29.attn_k.weight` | 2560×1024 | Q4_K | 2,180,545,792 | 1,474,560 |
| 380 | `blk.29.attn_k_norm.weight` | 256 | F32 | 2,182,020,352 | 1,024 |
| 381 | `blk.29.attn_norm.weight` | 2560 | F32 | 2,182,021,376 | 10,240 |
| 382 | `blk.29.attn_output.weight` | 2048×2560 | Q4_K | 2,182,031,616 | 2,949,120 |
| 383 | `blk.29.attn_q.weight` | 2560×2048 | Q4_K | 2,184,980,736 | 2,949,120 |
| 384 | `blk.29.attn_q_norm.weight` | 256 | F32 | 2,187,929,856 | 1,024 |
| 385 | `blk.29.attn_v.weight` | 2560×1024 | Q6_K | 2,187,930,880 | 2,150,400 |
| 386 | `blk.29.ffn_down.weight` | 10240×2560 | Q6_K | 2,190,081,280 | 21,504,000 |
| 387 | `blk.29.ffn_gate.weight` | 2560×10240 | Q4_K | 2,211,585,280 | 14,745,600 |
| 388 | `blk.29.ffn_norm.weight` | 2560 | F32 | 2,226,330,880 | 10,240 |
| 389 | `blk.29.ffn_up.weight` | 2560×10240 | Q4_K | 2,226,341,120 | 14,745,600 |
| 390 | `blk.29.post_attention_norm.weight` | 2560 | F32 | 2,241,086,720 | 10,240 |
| 391 | `blk.29.post_ffw_norm.weight` | 2560 | F32 | 2,241,096,960 | 10,240 |
| 392 | `blk.30.attn_k.weight` | 2560×1024 | Q4_K | 2,241,107,200 | 1,474,560 |
| 393 | `blk.30.attn_k_norm.weight` | 256 | F32 | 2,242,581,760 | 1,024 |
| 394 | `blk.30.attn_norm.weight` | 2560 | F32 | 2,242,582,784 | 10,240 |
| 395 | `blk.30.attn_output.weight` | 2048×2560 | Q4_K | 2,242,593,024 | 2,949,120 |
| 396 | `blk.30.attn_q.weight` | 2560×2048 | Q4_K | 2,245,542,144 | 2,949,120 |
| 397 | `blk.30.attn_q_norm.weight` | 256 | F32 | 2,248,491,264 | 1,024 |
| 398 | `blk.30.attn_v.weight` | 2560×1024 | Q6_K | 2,248,492,288 | 2,150,400 |
| 399 | `blk.30.ffn_down.weight` | 10240×2560 | Q6_K | 2,250,642,688 | 21,504,000 |
| 400 | `blk.30.ffn_gate.weight` | 2560×10240 | Q4_K | 2,272,146,688 | 14,745,600 |
| 401 | `blk.30.ffn_norm.weight` | 2560 | F32 | 2,286,892,288 | 10,240 |
| 402 | `blk.30.ffn_up.weight` | 2560×10240 | Q4_K | 2,286,902,528 | 14,745,600 |
| 403 | `blk.30.post_attention_norm.weight` | 2560 | F32 | 2,301,648,128 | 10,240 |
| 404 | `blk.30.post_ffw_norm.weight` | 2560 | F32 | 2,301,658,368 | 10,240 |
| 405 | `blk.31.attn_k.weight` | 2560×1024 | Q4_K | 2,301,668,608 | 1,474,560 |
| 406 | `blk.31.attn_k_norm.weight` | 256 | F32 | 2,303,143,168 | 1,024 |
| 407 | `blk.31.attn_norm.weight` | 2560 | F32 | 2,303,144,192 | 10,240 |
| 408 | `blk.31.attn_output.weight` | 2048×2560 | Q4_K | 2,303,154,432 | 2,949,120 |
| 409 | `blk.31.attn_q.weight` | 2560×2048 | Q4_K | 2,306,103,552 | 2,949,120 |
| 410 | `blk.31.attn_q_norm.weight` | 256 | F32 | 2,309,052,672 | 1,024 |
| 411 | `blk.31.attn_v.weight` | 2560×1024 | Q6_K | 2,309,053,696 | 2,150,400 |
| 412 | `blk.31.ffn_down.weight` | 10240×2560 | Q6_K | 2,311,204,096 | 21,504,000 |
| 413 | `blk.31.ffn_gate.weight` | 2560×10240 | Q4_K | 2,332,708,096 | 14,745,600 |
| 414 | `blk.31.ffn_norm.weight` | 2560 | F32 | 2,347,453,696 | 10,240 |
| 415 | `blk.31.ffn_up.weight` | 2560×10240 | Q4_K | 2,347,463,936 | 14,745,600 |
| 416 | `blk.31.post_attention_norm.weight` | 2560 | F32 | 2,362,209,536 | 10,240 |
| 417 | `blk.31.post_ffw_norm.weight` | 2560 | F32 | 2,362,219,776 | 10,240 |
| 418 | `blk.32.attn_k.weight` | 2560×1024 | Q4_K | 2,362,230,016 | 1,474,560 |
| 419 | `blk.32.attn_k_norm.weight` | 256 | F32 | 2,363,704,576 | 1,024 |
| 420 | `blk.32.attn_norm.weight` | 2560 | F32 | 2,363,705,600 | 10,240 |
| 421 | `blk.32.attn_output.weight` | 2048×2560 | Q4_K | 2,363,715,840 | 2,949,120 |
| 422 | `blk.32.attn_q.weight` | 2560×2048 | Q4_K | 2,366,664,960 | 2,949,120 |
| 423 | `blk.32.attn_q_norm.weight` | 256 | F32 | 2,369,614,080 | 1,024 |
| 424 | `blk.32.attn_v.weight` | 2560×1024 | Q6_K | 2,369,615,104 | 2,150,400 |
| 425 | `blk.32.ffn_down.weight` | 10240×2560 | Q6_K | 2,371,765,504 | 21,504,000 |
| 426 | `blk.32.ffn_gate.weight` | 2560×10240 | Q4_K | 2,393,269,504 | 14,745,600 |
| 427 | `blk.32.ffn_norm.weight` | 2560 | F32 | 2,408,015,104 | 10,240 |
| 428 | `blk.32.ffn_up.weight` | 2560×10240 | Q4_K | 2,408,025,344 | 14,745,600 |
| 429 | `blk.32.post_attention_norm.weight` | 2560 | F32 | 2,422,770,944 | 10,240 |
| 430 | `blk.32.post_ffw_norm.weight` | 2560 | F32 | 2,422,781,184 | 10,240 |
| 431 | `blk.33.attn_k.weight` | 2560×1024 | Q4_K | 2,422,791,424 | 1,474,560 |
| 432 | `blk.33.attn_k_norm.weight` | 256 | F32 | 2,424,265,984 | 1,024 |
| 433 | `blk.33.attn_norm.weight` | 2560 | F32 | 2,424,267,008 | 10,240 |
| 434 | `blk.33.attn_output.weight` | 2048×2560 | Q4_K | 2,424,277,248 | 2,949,120 |
| 435 | `blk.33.attn_q.weight` | 2560×2048 | Q4_K | 2,427,226,368 | 2,949,120 |
| 436 | `blk.33.attn_q_norm.weight` | 256 | F32 | 2,430,175,488 | 1,024 |
| 437 | `blk.33.attn_v.weight` | 2560×1024 | Q6_K | 2,430,176,512 | 2,150,400 |
| 438 | `blk.33.ffn_down.weight` | 10240×2560 | Q6_K | 2,432,326,912 | 21,504,000 |
| 439 | `blk.33.ffn_gate.weight` | 2560×10240 | Q4_K | 2,453,830,912 | 14,745,600 |
| 440 | `blk.33.ffn_norm.weight` | 2560 | F32 | 2,468,576,512 | 10,240 |
| 441 | `blk.33.ffn_up.weight` | 2560×10240 | Q4_K | 2,468,586,752 | 14,745,600 |
| 442 | `blk.33.post_attention_norm.weight` | 2560 | F32 | 2,483,332,352 | 10,240 |
| 443 | `blk.33.post_ffw_norm.weight` | 2560 | F32 | 2,483,342,592 | 10,240 |

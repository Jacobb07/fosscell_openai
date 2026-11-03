# Data Processor

A high-throughput command-line processor for request-log data. It reads `requests.log`, groups records by UTC hour and endpoint, calculates latency percentiles and HTTP 5xx error metrics, and writes the results to `summary.txt`.

> **Scope note:** The supplied `data_processor.py` does not define a `DataProcessor` class and does not implement general-purpose numerical dataset operations, variance, standard deviation, filtering, linear transformations, or CSV persistence. The README requirements in the supplied `INSTRUCTIONS.md` describe a different API. This document describes the implementation that is actually present in `data_processor.py`, rather than documenting methods that do not exist.

## Overview

The program is designed to summarize large request logs efficiently:

- Reads records from `requests.log`.
- Groups records by the hour in the timestamp and by endpoint.
- Counts requests and HTTP 5xx responses for each group.
- Calculates the 50th, 95th, and 99th percentile latency values.
- Calculates the 5xx error rate in parts per million (PPM).
- Writes one summary row per hour/endpoint group to `summary.txt`.

The implementation has an optimized byte-oriented processing path. It reads the input in chunks, can distribute parsing and percentile work across child processes on supported Unix-like systems, and falls back to a simpler reference implementation when the input does not meet the fast path's assumptions.

## Requirements

- Python **3.7 or newer** is the intended baseline.
- The implementation uses Python standard-library modules only:
  - `gc`
  - `marshal`
  - `os`
  - `sys`
  - `array`
  - `bisect`
  - `itertools`
  - `datetime` (loaded by the fallback implementation)

The multiprocessing fast path uses `os.fork`, `os.pipe`, and `os.pread`, which are Unix-oriented APIs. The reference path is simpler and does not require the fork-based worker model, but the script's automatic fallback is triggered by data/processing conditions rather than by a documented cross-platform compatibility guarantee.

## Input format

By default, the program reads:

```text
/data/requests.log
```

Each record must contain five whitespace-separated fields:

```text
timestamp method endpoint status latency_us
```

For example:

```text
2025-01-15T10:12:30.123Z GET /api/items 200 1250
2025-01-15T10:13:04.456Z POST /api/items 503 8400
2025-01-15T11:02:17.000Z GET /health 200 250
```

Field meanings:

| Field | Description |
|---|---|
| `timestamp` | Request timestamp. The fast path expects a UTC-style timestamp ending in `Z`; the fallback parses timestamps using `%Y-%m-%dT%H:%M:%S.%f%z`. |
| `method` | HTTP method. Parsed but not used in aggregation. |
| `endpoint` | Endpoint key used to group requests. |
| `status` | HTTP status code. Codes from 500 through 599 count as errors. |
| `latency_us` | Integer request latency, in microseconds. |

The input is expected to have exactly five fields per line. The fast path is optimized for timestamps ending in `Z`, with a fractional component, and for latency values that fit in an unsigned 32-bit integer. If its assumptions are not met, it raises an internal fallback signal and the reference implementation is attempted.

## Output format

By default, the program writes:

```text
/out/summary.txt
```

Each output line has this space-separated format:

```text
YYYY-MM-DDTHH:00:00Z endpoint request_count p50 p95 p99 error_count error_rate_ppm
```

Example:

```text
2025-01-15T10:00:00Z /api/items 2 1250 8400 8400 1 500000
2025-01-15T11:00:00Z /health 1 250 250 250 0 0
```

Columns:

| Column | Meaning |
|---|---|
| Hour | Hour bucket represented as `YYYY-MM-DDTHH:00:00Z`. |
| Endpoint | Request endpoint. |
| `request_count` | Number of records in the group. |
| `p50` | 50th percentile latency, in microseconds. |
| `p95` | 95th percentile latency, in microseconds. |
| `p99` | 99th percentile latency, in microseconds. |
| `error_count` | Number of responses with status 500–599. |
| `error_rate_ppm` | `error_count * 1,000,000 // request_count`. |

Percentiles use the nearest-rank convention: the rank is `ceil(percentile * count / 100)`, using one-based ranks into the sorted latency values.

## Setup and usage

Save `data_processor.py` in your project. No third-party packages need to be installed.

### Run with the default directories

```bash
python data_processor.py
```

This reads `/data/requests.log` and creates `/out/summary.txt`.

### Specify input and output directories

```bash
python data_processor.py --data ./data --out ./results
```

The input file must be named `requests.log` inside the data directory. The output file is always named `summary.txt` inside the output directory; the output directory is created if necessary.

The equivalent `--option=value` syntax is supported:

```bash
python data_processor.py --data=./data --out=./results
```

### Show command-line help

```bash
python data_processor.py --help
```

### Importing

The script can be imported as a Python module:

```python
import data_processor

data_processor.main("./data", "./results")
```

The module's `main(data_dir, out_dir)` function is the programmatic entry point. It does not expose a `DataProcessor` class.

## Configuration

Two environment variables control the optimized implementation:

| Variable | Default | Purpose |
|---|---:|---|
| `A_INGEST_CHUNK` | `262144` bytes (256 KiB) | Chunk size used when reading and parsing input. |
| `A_INGEST_WORKERS` | Automatically detected CPU budget | Overrides the number of worker processes used by the fast path. Values are converted to integers and clamped to at least one. |

Example:

```bash
A_INGEST_CHUNK=524288 A_INGEST_WORKERS=4 \
  python data_processor.py --data ./data --out ./results
```

When no worker override is set, the script estimates the available CPU budget and, where available, accounts for a cgroup CPU quota. Small input files are processed by one worker.

## Processing details

The main processing flow is:

1. Open the input log and determine its size.
2. Split the file into line-aligned ranges.
3. Read each range in chunks and parse its records.
4. Aggregate latency values and 5xx counts by `(hour, endpoint)`.
5. Calculate p50, p95, and p99 for each group.
6. Write the groups in sorted key order to `summary.txt`.

For larger workloads, the fast path uses child processes for parsing and/or percentile calculations. Percentile calculation can sort values in bounded pieces for large groups, and it includes a histogram-based optimization for groups with relatively few distinct latency values.

If the fast path encounters input that violates its assumptions, it switches to `reference_main`, which parses records with `datetime`, sorts each group's latency values, and calculates the same summary metrics.

## Python API reference

The module exposes functions rather than a `DataProcessor` class.

### `main(data_dir, out_dir)`

```python
main(data_dir: str, out_dir: str) -> None
```

Runs the processing pipeline. It first tries the optimized implementation. If the fast path signals that the input needs the reference implementation, it runs that implementation. If the optimized attempt fails for another exception, it retries in single-process mode and then attempts the reference implementation if needed.

- `data_dir`: Directory containing `requests.log`.
- `out_dir`: Directory in which `summary.txt` will be written.
- Returns: `None`; writes the output file as a side effect.
- Errors: The function has retry/fallback handling, but final filesystem, parsing, or resource errors may still propagate from the fallback path.

### `fast_main(data_dir, out_dir, single=False)`

```python
fast_main(data_dir: str, out_dir: str, single: bool = False) -> None
```

Runs the optimized processing path.

- `data_dir`: Directory containing `requests.log`.
- `out_dir`: Destination directory for `summary.txt`.
- `single`: If `True`, avoids parallel worker use.
- Returns: `None`.
- Potential errors: File and operating-system errors; internal `Fallback` when the input is outside fast-path assumptions; `WorkerFailed` when a child process fails unexpectedly.

### `reference_main(data_dir, out_dir)`

```python
reference_main(data_dir: str, out_dir: str) -> None
```

Runs the reference implementation using text parsing and in-memory sorting.

- `data_dir`: Directory containing `requests.log`.
- `out_dir`: Destination directory for `summary.txt`.
- Returns: `None`.
- Potential errors: `FileNotFoundError`, other file I/O errors, `ValueError` for malformed records, invalid timestamps, or invalid integer fields.

### `parse_chunk(buf, acc)`

```python
parse_chunk(buf: bytes, acc: dict) -> None
```

Parses a byte buffer containing complete log lines and updates the aggregation dictionary.

- `buf`: Input bytes containing complete lines.
- `acc`: Mutable aggregation dictionary, keyed by `(hour_prefix, endpoint)`.
- Returns: `None`.
- Potential errors: Raises the internal `Fallback` exception when the buffer does not match fast-path assumptions.

### `process_range(fd, a, b, acc)`

```python
process_range(fd: int, a: int, b: int, acc: dict) -> None
```

Reads and processes the file byte range `[a, b)` using `os.pread`.

- `fd`: Open file descriptor.
- `a`, `b`: Start and end byte offsets.
- `acc`: Mutable aggregation dictionary.
- Returns: `None`.
- Potential errors: `OSError` from file operations; `Fallback` if the file changes or parsing assumptions fail.

### `ranks_of(pieces)`

```python
ranks_of(pieces: list[bytes]) -> tuple[int, int, int, int]
```

Combines unsigned 32-bit latency buffers and returns the sample count and p50, p95, and p99 values.

- `pieces`: Byte buffers containing packed unsigned 32-bit integers.
- Returns: `(count, p50, p95, p99)`.
- Potential errors: Resource-related exceptions for very large inputs; malformed buffers may cause array conversion errors.

### `cpu_budget()`

```python
cpu_budget() -> int
```

Returns the detected worker budget, or the positive integer configured through `A_INGEST_WORKERS`.

### Command-line argument parsing

```python
_parse_args(argv: list[str]) -> tuple[str, str]
```

Parses `--data`, `--out`, `--data=...`, and `--out=...`. Defaults are `/data` and `/out`. `-h` and `--help` print usage and exit. Unknown options or missing option values terminate with an error message.

## Examples

### Prepare sample input and run the processor

Create a directory named `data` and save the following as `data/requests.log`:

```text
2025-01-15T10:12:30.123Z GET /api/items 200 1250
2025-01-15T10:13:04.456Z POST /api/items 503 8400
2025-01-15T11:02:17.000Z GET /health 200 250
```

Run:

```bash
python data_processor.py --data ./data --out ./out
```

Read the result:

```bash
cat ./out/summary.txt
```

Expected output:

```text
2025-01-15T10:00:00Z /api/items 2 1250 8400 8400 1 500000
2025-01-15T11:00:00Z /health 1 250 250 250 0 0
```

### Run from Python

```python
from data_processor import main

main("./data", "./out")
```

## Error handling and edge cases

- **Empty input file:** Produces an empty `summary.txt`.
- **Empty lines or malformed records:** The fast parser may reject the data and switch to the reference parser. The reference parser expects exactly five fields separated by single spaces after newline removal; malformed rows can raise `ValueError`.
- **Invalid timestamps:** The reference parser uses `datetime.strptime` with `%Y-%m-%dT%H:%M:%S.%f%z`; timestamps that do not match this format raise `ValueError`.
- **Invalid status or latency values:** Integer conversion failures raise `ValueError` in the reference path. The fast path falls back for latency values that cannot be represented as unsigned 32-bit integers.
- **Latency range:** The optimized path stores latency values in `array('I')`, so values must fit the platform's unsigned-int array representation (the implementation is designed around 32-bit values). The reference path uses Python integers.
- **5xx detection:** Only status codes from 500 through 599 are counted as errors. Other status codes are not counted.
- **Missing input file:** Opening a missing `requests.log` raises `FileNotFoundError`.
- **Output directory:** The program creates the output directory if it does not already exist. Permission and other filesystem errors can still occur.
- **Input mutation during processing:** The fast path checks read lengths and can fall back if the file changes while it is being read.
- **Worker failure:** If a child process fails unexpectedly, the main function retries the optimized path in single-process mode before attempting the reference implementation.

## Limitations

- The output is a space-separated text file, not CSV.
- The method field is parsed but not included in grouping or output.
- The module does not provide a stateful `DataProcessor` object.
- It does not implement mean, median, variance, standard deviation, arbitrary filtering, or linear transformations.
- The fast path is optimized for the timestamp and latency assumptions described above; other valid input forms may be handled by the reference path, subject to its timestamp and record-format requirements.

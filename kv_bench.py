#!/usr/bin/env python3
"""
KV Store Benchmark — Redis-style, multiprocessing
Usage: python3 kv_bench.py <server_ip> [ops_per_client] [num_clients] [SET|GET|MIXED]

Defaults (mirrors redis-benchmark):
  ops_per_client : 2000   (50 clients x 2000 = 100,000 total)
  num_clients    : 50
  mode           : MIXED  (80% SET / 20% GET)
"""

import socket
import struct
import random
import string
import time
import sys
from multiprocessing import Pool

# ── Config ────────────────────────────────────────────────────────────────────
SERVER_IP      = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
OPS_PER_CLIENT = int(sys.argv[2])   if len(sys.argv) > 2 else 2000
NUM_CLIENTS    = int(sys.argv[3])   if len(sys.argv) > 3 else 50
MODE           = sys.argv[4].upper() if len(sys.argv) > 4 else "MIXED"
SERVER_PORT    = 8080
KEY_POOL       = 1_000_000

# ── Protocol ──────────────────────────────────────────────────────────────────
def send_set(sock, key: bytes, val: bytes):
    pkt = struct.pack("BBB", 0x01, len(key), len(val)) + key + val
    sock.sendall(pkt)
    resp = sock.recv(1)
    return resp[0] if resp else -1

def send_get(sock, key: bytes):
    pkt = struct.pack("BB", 0x02, len(key)) + key
    sock.sendall(pkt)
    resp = sock.recv(1)
    if not resp:
        return -1, None
    status = resp[0]
    if status == 0x00:
        vlen_b = sock.recv(1)
        if not vlen_b:
            return status, None
        vlen = vlen_b[0]
        val = b""
        while len(val) < vlen:
            chunk = sock.recv(vlen - len(val))
            if not chunk:
                break
            val += chunk
        return status, val
    return status, None

# ── Helpers ───────────────────────────────────────────────────────────────────
def make_key(idx: int) -> bytes:
    return f"k{idx:011d}".encode()

def make_val() -> bytes:
    return ''.join(random.choices(string.ascii_letters + string.digits, k=8)).encode()

# ── Worker ────────────────────────────────────────────────────────────────────
def worker(args):
    client_id, ops, ip, port, mode = args
    completed = errors = 0

    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect((ip, port))
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    except Exception as e:
        print(f"[client {client_id}] connect failed: {e}", flush=True)
        return (0, 1)

    try:
        if mode == "SET":
            for _ in range(ops):
                key = make_key(random.randint(0, KEY_POOL - 1))
                r = send_set(sock, key, make_val())
                completed += 1 if r != -1 else 0
                errors    += 1 if r == -1 else 0

        elif mode == "GET":
            for _ in range(ops):
                key = make_key(random.randint(0, KEY_POOL - 1))
                status, _ = send_get(sock, key)
                completed += 1 if status != -1 else 0
                errors    += 1 if status == -1 else 0

        else:  # MIXED 80/20
            set_ops = int(ops * 0.8)
            get_ops = ops - set_ops
            for _ in range(set_ops):
                key = make_key(random.randint(0, KEY_POOL - 1))
                r = send_set(sock, key, make_val())
                completed += 1 if r != -1 else 0
                errors    += 1 if r == -1 else 0
            for _ in range(get_ops):
                key = make_key(random.randint(0, KEY_POOL - 1))
                status, _ = send_get(sock, key)
                completed += 1 if status != -1 else 0
                errors    += 1 if status == -1 else 0

    except Exception as e:
        print(f"[client {client_id}] error: {e}", flush=True)
        errors += 1
    finally:
        sock.close()

    return (completed, errors)

# ── Main ──────────────────────────────────────────────────────────────────────
def main():
    total_ops = NUM_CLIENTS * OPS_PER_CLIENT
    print(f"Server        : {SERVER_IP}:{SERVER_PORT}")
    print(f"Clients       : {NUM_CLIENTS}")
    print(f"Ops/client    : {OPS_PER_CLIENT}")
    print(f"Total ops     : {total_ops:,}")
    print(f"Mode          : {MODE}")
    print(f"Key pool      : {KEY_POOL:,}")
    print("---")

    args = [(i, OPS_PER_CLIENT, SERVER_IP, SERVER_PORT, MODE) for i in range(NUM_CLIENTS)]

    t_start = time.perf_counter()
    with Pool(processes=NUM_CLIENTS) as pool:
        results = pool.map(worker, args)
    t_end = time.perf_counter()

    completed = sum(r[0] for r in results)
    errors    = sum(r[1] for r in results)
    elapsed   = t_end - t_start
    ops_sec   = completed / elapsed if elapsed > 0 else 0

    print(f"=== {MODE} RESULTS ===")
    print(f"Completed ops : {completed:,} / {total_ops:,}")
    print(f"Errors        : {errors:,}")
    print(f"Elapsed       : {elapsed:.3f}s")
    print(f"Ops/sec       : {ops_sec:,.0f}")

if __name__ == "__main__":
    main()

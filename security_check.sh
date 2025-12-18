#!/bin/bash

set -e

echo "Running cargo audit for vulnerabilities..."
cargo audit

echo "Running clippy with security and pedantic lints..."
cargo clippy -- -W clippy::security -W clippy::pedantic -W clippy::nursery

echo "Running fuzz tests for crypto..."
cd rust/crypto
cargo fuzz run encrypt_decrypt -- -runs=10000
cargo fuzz run fingerprint -- -runs=10000
cargo fuzz run pin -- -runs=10000
cargo fuzz run shared_secret -- -runs=10000

echo "Security checks completed."
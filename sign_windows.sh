#!/bin/bash

# Script to sign Windows MSI

set -e

MSI_FILE="$1"
CERT_FILE="cert.pfx"  # Path to certificate
CERT_PASSWORD="password"  # Certificate password

signtool sign /f "$CERT_FILE" /p "$CERT_PASSWORD" "$MSI_FILE"
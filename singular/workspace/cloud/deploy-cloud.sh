#!/usr/bin/env bash
set -euo pipefail
echo "[SINGULAR Ω v33.0] Robotic Pulumi + Docker Multi-Cloud Agent Farm Deployment..."
pulumi up --yes --cwd "$(dirname "$0")"

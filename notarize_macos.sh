#!/bin/bash

# Script to notarize the macOS DMG

set -e

DMG_FILE="$1"
BUNDLE_ID="com.bluebeam.app"  # Replace with actual bundle ID
USERNAME="your-apple-id@example.com"  # Replace with Apple ID
PASSWORD="@keychain:Developer-altool"  # Or use app-specific password

# Submit for notarization
xcrun altool --notarize-app --primary-bundle-id "$BUNDLE_ID" --username "$USERNAME" --password "$PASSWORD" --file "$DMG_FILE"

# Wait for notarization to complete (this is simplistic; in practice, poll the status)
echo "Notarization submitted. Check status manually or implement polling."

# Once approved, staple the ticket
xcrun stapler staple "$DMG_FILE"
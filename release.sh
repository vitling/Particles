#!/usr/bin/env bash

set -ex

if [[ -z "$SIGNING_CERTIFICATE_P12_DATA" ]]; then
    echo "Base64 encoded P12 certificate data in SIGNING_CERTIFICATE_P12_DATA required";
    exit 1;
fi

if [[ -z "$SIGNING_CERTIFICATE_PASSWORD" ]]; then
    echo "SIGNING_CERTIFICATE_PASSWORD required";
    exit 1;
fi

rm -r build-release
mkdir build-release
cmake -E make_directory build-release

cd build-release

# Adapted from https://medium.com/better-programming/indie-mac-app-devops-with-github-actions-b16764a3ebe7

KEYCHAIN_FILE=default.keychain
KEYCHAIN_PASSWORD=temporary_single_use_password

security create-keychain -p $KEYCHAIN_PASSWORD $KEYCHAIN_FILE
security default-keychain -s $KEYCHAIN_FILE
security unlock-keychain -p $KEYCHAIN_PASSWORD $KEYCHAIN_FILE
security import <(echo "$SIGNING_CERTIFICATE_P12_DATA" | base64 --decode) \
    -f pkcs12 \
    -k $KEYCHAIN_FILE \
    -P "$SIGNING_CERTIFICATE_PASSWORD" \
    -T /usr/bin/codesign

security set-key-partition-list -S apple-tool:,apple: -s -k $KEYCHAIN_PASSWORD $KEYCHAIN_FILE

cmake ../ -DCMAKE_BUILD_TYPE=Release
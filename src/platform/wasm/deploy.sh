#!/bin/bash
set -e
set -x

git init
echo mgba.jarhar.com > CNAME
git add -f .
git commit -m "github pages"
git checkout -b pages
git remote add origin git@github.com:josepharhar/mgba
git push -f origin pages
rm CNAME
rm -rf .git

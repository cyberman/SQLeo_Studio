#!/bin/sh

path=trunk
if [ "$1" != "" ]; then
    path=branches/$1
fi

OLDDIR=`pwd`

TEMP=`mktemp -d`
cd $TEMP

git clone https://github.com/pawelsalawa/letos.git letos

cd letos
rm -rf .git .gitignore

VERSION_INT=`cat Letos/core/letos.cpp | grep static | grep letosVersion | sed 's/\;//'`
VERSION=`echo $VERSION_INT | awk '{print int($6/10000) "." int($6/100%100) "." int($6%100)}'`

tar cf ../letos-$VERSION.tar Letos Plugins
gzip -9 ../letos-$VERSION.tar

zip -r ../letos-$VERSION.zip Letos Plugins

cd "$OLDDIR"

mv $TEMP/letos-$VERSION.zip ../output
mv $TEMP/letos-$VERSION.tar.gz ../output

cd ../output

rm -rf $TEMP

echo "Source packages stored in `pwd`"

cd "$OLDDIR"

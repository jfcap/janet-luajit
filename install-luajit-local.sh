# build and install LuaJit-2.1 locally

TARGET=$PWD

cd $TARGET && git clone http://luajit.org/git/luajit-2.0.git
cd $TARGET/luajit-2.0 && git checkout v2.1 && make PREFIX=$TARGET &&  make PREFIX=$TARGET install
ln -sf $TARGET/bin/luajit-2.1.0-beta3 $TARGET/bin/luajit		    
cd $TARGET && rm -fr $TARGET/luajit-2.0












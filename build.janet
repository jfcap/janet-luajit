(import cook)

(cook/make-native
 :name "lua"
 :cflags (string cook/CFLAGS " -I./include/luajit-2.1 ")
 :lflags " -L./lib -lluajit-5.1"
 :source @["janet_lua.c"])




(print "\nrun some tests")
(eval-string (slurp "test.janet"))

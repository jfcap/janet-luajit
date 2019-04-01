(import cook)

(eval-string (slurp "configure.janet"))

(cook/make-native
 :name "lua"
 :cflags (string/format " %s -I%s" cook/CFLAGS LUAJIT_INC)
 :lflags (string/format " -L%s -l%s" LUAJIT_LIB LUAJIT_LIBNAME)
 :source @["janet_lua.c"])


(print "\nrun some tests")
(eval-string (slurp "test.janet"))

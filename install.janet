(defn- sys-install
       [& files]
       (let (cp (fn [src dest]
                  (print "copy " src " to " dest)
                  (spit dest (slurp src))))  
         (each f files
               (cp (string "./build/" f)
                   (string module/*syspath* "/" f)))))

(sys-install "lua.so")


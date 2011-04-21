#!/bin/sh

patch -p2 <<'EOF'
--- old/postgresql/server/utils/syncbitvector.h	2010-11-18 15:07:30.000000000 -0800
+++ new/postgresql/server/utils/syncbitvector.h	2011-04-20 23:29:41.000000000 -0700
@@ -16,7 +16,7 @@
 #define SYNC_BITVECTOR_MAX_COUNT 	(100)								/* max total size = 100 MB */
 
 /* bit vector container */
-struct SyncBitvectorContainer_s
+typedef struct SyncBitvectorContainer_s
 {
 	int64 vectorCount;		// number of vectors stored
 
EOF

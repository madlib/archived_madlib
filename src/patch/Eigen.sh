#!/bin/sh

# Patch directly in the source tree. Ignore if the patch has already been
# applied. (Probably there are situations where CMake isn't smart enought to
# prevent that.)

# The patch makes Eigen use MADlib's memory-allocation routines
patch -N -p1 <<'EOF'
--- old/Eigen/src/Core/util/Memory.h	2011-10-26 21:01:39.000000000 -0700
+++ new/Eigen/src/Core/util/Memory.h	2011-10-27 11:06:00.000000000 -0700
@@ -199,7 +199,9 @@
   check_that_malloc_is_allowed();
 
   void *result;
-  #if !EIGEN_ALIGN
+  #if 1
+    result = madlib::dbal::defaultAllocator().allocate(size);
+  #elif !EIGEN_ALIGN
     result = std::malloc(size);
   #elif EIGEN_MALLOC_ALREADY_ALIGNED
     result = std::malloc(size);
@@ -223,7 +225,9 @@
 /** \internal Frees memory allocated with aligned_malloc. */
 inline void aligned_free(void *ptr)
 {
-  #if !EIGEN_ALIGN
+  #if 1
+    madlib::dbal::defaultAllocator().free(ptr);
+  #elif !EIGEN_ALIGN
     std::free(ptr);
   #elif EIGEN_MALLOC_ALREADY_ALIGNED
     std::free(ptr);
@@ -248,7 +252,9 @@
   EIGEN_UNUSED_VARIABLE(old_size);
 
   void *result;
-#if !EIGEN_ALIGN
+#if 1
+  result = madlib::dbal::defaultAllocator().reallocate(ptr, new_size);
+#elif !EIGEN_ALIGN
   result = std::realloc(ptr,new_size);
 #elif EIGEN_MALLOC_ALREADY_ALIGNED
   result = std::realloc(ptr,new_size);
@@ -292,7 +298,7 @@
 {
   check_that_malloc_is_allowed();
 
-  void *result = std::malloc(size);
+  void *result = madlib::dbal::defaultAllocator().allocate(size);
   #ifdef EIGEN_EXCEPTIONS
     if(!result) throw std::bad_alloc();
   #endif
@@ -307,7 +313,7 @@
 
 template<> inline void conditional_aligned_free<false>(void *ptr)
 {
-  std::free(ptr);
+  madlib::dbal::defaultAllocator().free(ptr);
 }
 
 template<bool Align> inline void* conditional_aligned_realloc(void* ptr, size_t new_size, size_t old_size)
@@ -317,7 +323,7 @@
 
 template<> inline void* conditional_aligned_realloc<false>(void* ptr, size_t new_size, size_t)
 {
-  return std::realloc(ptr, new_size);
+  return madlib::dbal::defaultAllocator().reallocate(ptr, new_size);
 }
 
 /*****************************************************************************
EOF

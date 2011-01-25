\qecho =================================================================
\qecho === Running: sketches ===========================================
\qecho =================================================================
\qecho 

\qecho === CM : count ==================================================
SELECT pronamespace, madlib.cmsketch_count(pronargs, 3)
     FROM pg_proc
 GROUP BY pronamespace;

\qecho === CM : rangecount =============================================
SELECT pronamespace, madlib.cmsketch_rangecount(pronargs, 3, 5)
     FROM pg_proc
 GROUP BY pronamespace;

\qecho === CM : centile ================================================
SELECT relnamespace, madlib.cmsketch_centile(oid::int8, 75)
     FROM pg_class
 GROUP BY relnamespace;

\qecho === CM : cmsketch_depth_histogram ===============================
SELECT madlib.cmsketch_width_histogram(madlib.cmsketch(oid::int8), min(oid::int8), max(oid::int8), 10)
     FROM pg_class;

\qecho === CM : count ==================================================
SELECT madlib.cmsketch_depth_histogram(oid::int8, 10)
     FROM pg_class;

\qecho === FM : distinct count =========================================
SELECT pronargs, madlib.fmsketch_dcount(proname) AS distinct_hat, count(proname)
      FROM pg_proc
  GROUP BY pronargs;

\qecho === MFV : mfvsketch_top_histogram ================================
SELECT madlib.mfvsketch_top_histogram(proname, 4)
     FROM pg_proc;

\qecho === MFV : mfvsketch_quick_histogram ==============================
SELECT madlib.mfvsketch_quick_histogram(proname, 4)
     FROM pg_proc;

/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_graph_test.c - Rasqal RDF Query GRAPH Tests
 *
 * Copyright (C) 2006, David Beckett http://purl.org/net/dajobe/
 * 
 * This package is Free Software and part of Redland http://librdf.org/
 * 
 * It is licensed under the following three licenses as alternatives:
 *   1. GNU Lesser General Public License (LGPL) V2.1 or any newer version
 *   2. GNU General Public License (GPL) V2 or any newer version
 *   3. Apache License, V2.0 or any newer version
 * 
 * You may not use this file except in compliance with at least one of
 * the above three licenses.
 * 
 * See LICENSE.html or LICENSE.txt at the top of this package for the
 * complete terms and further detail along with the license texts for
 * the licenses in COPYING.LIB, COPYING and LICENSE-2.0.txt respectively.
 * 
 * 
 */

#ifdef HAVE_CONFIG_H
#include <rasqal_config.h>
#endif

#ifdef WIN32
#include <win32_rasqal_config.h>
#endif

#include <stdio.h>
#include <string.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <stdarg.h>

#include "rasqal.h"
#include "rasqal_internal.h"

#ifdef RASQAL_QUERY_SPARQL

#define QUERY_LANGUAGE "sparql"

#define QUERY_COUNT 1
static int expected_count[QUERY_COUNT]={ 2 };

static const char* answers[QUERY_COUNT][2] = {
  { "mercury", "orange" }
};
static const int graph_answers[QUERY_COUNT][2] = {
  { 0, 1 }
};

#define GRAPH_COUNT 2
static const char* graph_files[GRAPH_COUNT] = {
  "graph-a.ttl",
  "graph-b.ttl"
};

#define QUERY_VALUE_VAR ((const unsigned char *)"value")
#define QUERY_GRAPH_VAR ((const unsigned char *)"graph")

#define QUERY_FORMAT "\
PREFIX : <http://example.org/>\
SELECT ?graph ?value \
WHERE\
{\
  GRAPH ?graph { :x :b ?value } \
}\
"

#else
#define NO_QUERY_LANGUAGE
#endif


#ifdef NO_QUERY_LANGUAGE
int
main(int argc, char **argv) {
  const char *program=rasqal_basename(argv[0]);
  fprintf(stderr, "%s: No supported query language available, skipping test\n", program);
  return(0);
}
#else

int
main(int argc, char **argv) {
  const char *program=rasqal_basename(argv[0]);
  int failures=0;
  raptor_uri *base_uri;
  unsigned char *data_dir_string;
  raptor_uri* data_dir_uri;
  unsigned char *uri_string;
  int i;
  raptor_uri* graph_uris[GRAPH_COUNT];
  
  rasqal_init();
  
  if(argc != 2) {
    fprintf(stderr, "USAGE: %s <path to data directory>\n", program);
    return(1);
  }

  uri_string=raptor_uri_filename_to_uri_string("");
  base_uri=raptor_new_uri(uri_string);  
  raptor_free_memory(uri_string);


  data_dir_string=raptor_uri_filename_to_uri_string(argv[1]);
  data_dir_uri=raptor_new_uri(data_dir_string);
  for(i=0; i < GRAPH_COUNT; i++)
    graph_uris[i]=raptor_new_uri_relative_to_base(data_dir_uri,
                                                  (const unsigned char*)graph_files[i]);
  
  

  for(i=0; i < QUERY_COUNT; i++) {
    rasqal_query *query = NULL;
    rasqal_query_results *results = NULL;
    const char *query_language_name=QUERY_LANGUAGE;
    const unsigned char *query_format=(const unsigned char *)QUERY_FORMAT;
    int count;
    int query_failed=0;
    int j;

    query=rasqal_new_query(query_language_name, NULL);
    if(!query) {
      fprintf(stderr, "%s: creating query %d in language %s FAILED\n", 
              program, i, query_language_name);
      query_failed=1;
      goto tidy_query;
    }

    printf("%s: preparing %s query %d\n", program, query_language_name, i);
    if(rasqal_query_prepare(query, query_format, base_uri)) {
      fprintf(stderr, "%s: %s query prepare %d FAILED\n", program, 
              query_language_name, i);
      query_failed=1;
      goto tidy_query;
    }

    for(j=0; j < GRAPH_COUNT; j++)
      rasqal_query_add_data_graph(query, graph_uris[j], graph_uris[j],
                                  RASQAL_DATA_GRAPH_NAMED);
    

    printf("%s: executing query %d\n", program, i);
    results=rasqal_query_execute(query);
    if(!results) {
      fprintf(stderr, "%s: query execution %d FAILED\n", program, i);
      query_failed=1;
      goto tidy_query;
    }

    printf("%s: checking query %d results\n", program, i);
    count=0; 
    query_failed=0;
    while(results && !rasqal_query_results_finished(results)) {
      rasqal_literal *value;
      rasqal_literal *graph_value;
      raptor_uri* graph_uri;
      const char *value_answer=answers[i][count];
      raptor_uri* graph_answer=graph_uris[graph_answers[i][count]];

      value=rasqal_query_results_get_binding_value_by_name(results, 
                                                           QUERY_VALUE_VAR);
      if(strcmp((const char*)rasqal_literal_as_string(value), value_answer)) {
        printf("result %d FAILED: %s=", count, (char*)QUERY_VALUE_VAR);
        rasqal_literal_print(value, stdout);
        printf(" expected value '%s'\n", value_answer);
        query_failed=1;
        break;
      }

      graph_value=rasqal_query_results_get_binding_value_by_name(results, 
                                                                 QUERY_GRAPH_VAR);
      if(!graph_value) {
        printf("variable '%s' is not in the result\n", QUERY_GRAPH_VAR);
        query_failed=1;
        break;
      }
      
      if(graph_value->type != RASQAL_LITERAL_URI) {
        printf("variable '%s' is type %d expected %d\n", QUERY_GRAPH_VAR,
               graph_value->type, RASQAL_LITERAL_URI);
        query_failed=1;
        break;
      }
      
      graph_uri=graph_value->value.uri;
      if(!raptor_uri_equals(graph_uri, graph_answer)) {
        printf("result %d FAILED: %s=", count, (char*)QUERY_GRAPH_VAR);
        rasqal_literal_print(graph_value, stdout);
        printf(" expected URI value <%s>\n", 
               raptor_uri_as_string(graph_answer));
        query_failed=1;
        break;
      }

      rasqal_query_results_next(results);
      count++;
    }
    if(results)
      rasqal_free_query_results(results);

    printf("%s: checking query %d results count\n", program, i);
    if(count != expected_count[i]) {
      printf("%s: query execution %d FAILED returning %d results, expected %d\n", 
             program, i, count, expected_count[i]);
      query_failed=1;
    }

  tidy_query:

    rasqal_free_query(query);

    if(!query_failed)
      printf("%s: query %d OK\n", program, i);
    else {
      printf("%s: query %d FAILED\n", program, i);
      failures++;
    }
  }

  for(i=0; i < GRAPH_COUNT; i++) {
    if(graph_uris[i])
      raptor_free_uri(graph_uris[i]);
  }
  raptor_free_uri(data_dir_uri);
  raptor_free_memory(data_dir_string);
  
  raptor_free_uri(base_uri);

  rasqal_finish();

  return failures;
}

#endif

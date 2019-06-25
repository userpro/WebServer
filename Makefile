all:
	gcc main.c server.c http.c utils.c ./vector/qwq_string.c ./vector/qwq_hashmap.c ./vector/qwq_mix.c ./vector/qwq_utils.c fastcgi.c -pthread -Wall
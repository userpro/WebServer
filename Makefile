all:
	gcc main.c server.c http.c utils.c ./vendor/qwq_string.c ./vendor/qwq_hashmap.c ./vendor/qwq_mix.c ./vendor/qwq_utils.c fastcgi.c -pthread -Wall
./player: src/*.c
	gcc src/*.c -o ./player \
		-lm -lraylib \
	    -lavcodec -lavformat -lavutil -lswresample -lswscale \
		vendor/raylib-media/src/*.c -Ivendor/raylib-media/src

.PHONY: clean
clean:
	rm ./player

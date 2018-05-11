from uwimg import *

# 1. Getting and setting pixels
im = load_image("data/dog.jpg")
im = gl_rgb_scale(im, 2, 1, 1)
save_image(im, "dog_hue_shift")



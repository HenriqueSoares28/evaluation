#!/bin/bash

EXT="pgm" # extension of superpixel map images
IMG_PATH="data/original" # original images path
GT_PATH="data/gt" # gt images path
SPX_MAP_PATH="data\disf" # superpixel map images path
SAVE_PATH="ou" # save path
MIN_SPX_SIZE=70  # minimum superpixel size

mkdir -p ${SAVE_PATH}

PARAMS="--ext ${EXT} "
PARAMS+="--eval 10 "
PARAMS+="--img ${IMG_PATH} "
PARAMS+="--gt ${GT_PATH} "
PARAMS+="--rmsize ${MIN_SPX_SIZE} "
PARAMS+="--save ${SAVE_PATH} "
PARAMS+="--label ${SPX_MAP_PATH} "

./bin/main ${PARAMS}

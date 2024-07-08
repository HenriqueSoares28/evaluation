#!/bin/bash

EXT="pgm" # extension of superpixel map images
IMG_PATH="../datasets/Landsat-8_pca/images" # original images path
GT_PATH="../datasets/Landsat-8_pca/gt" # gt images path
SPX_MAP_PATH="../RESULTS/Segm/DISF/Landsat-8_pca/10000/" # superpixel map images path
SAVE_PATH="../RESULTS/Segm/DISF/Landsat-8_pca_MinSpx70/10000/" # save path
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

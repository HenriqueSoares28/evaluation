
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <iostream>
#include <vector>
// #include "Eval.h"

#include "PrioQueue.h"
#include "Image.h"
#include "Utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include "ift.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using namespace cv;
using namespace std;

// #define DEBUG 1

//===============================================================
int getNumSuperpixels(iftImage *L);
double *getImageVariance_channels(iftImage *image, iftImage *labels);
int relabelSuperpixels(int *labels, int connectivity);
int enforceNumSuperpixel(iftImage *labels, iftImage *image, int numDesiredSpx);

void RBD(iftImage *image, iftImage *labels, int label, int nbuckets, int *alpha, double **Descriptor);
double *SIRS(iftImage *labels, iftImage *image, int alpha, int nbuckets, char *reconFile, double gauss_variance, double *score);
double *computeExplainedVariation(int *labels, iftImage *image, char *reconFile, double *score);

bool is4ConnectedBoundaryPixel(iftImage *img, int i, int j, iftImage *labels);

void computeIntersectionMatrix(iftImage *labels, iftImage *gt,
                               int **intersection_matrix, int *superpixel_sizes, int *gt_sizes, int cols, int rows);
double computeBoundaryRecall(iftImage *labels, iftImage *gt, float d);
double computeUndersegmentationError(iftImage *labels, iftImage *gt);
double computeCompactness(iftImage *labels);

void getImageName(char *fullImgPath, char *fileName);

int mergeSpxBasedOnSize(iftImage *labels, iftImage *orig_img, int min_size,
                        iftImage *gt_image, iftColor color, char *tmp_save_path);
//===============================================================

typedef struct TextsConfig
{
    Vec3b textColor;
    char text[10];
} TextsConfig;

typedef struct Args
{
    char *img_path, *label_path, *label_path2, *label_ext, *gt_path;
    char *imgScoresPath, *imgRecon;
    char *logFile, *dLogFile, *saveLabels;
    int buckets, alpha, metric, k;
    int removeColor, removeSize, recreateLabels;
    bool drawScores;
    double gauss_variance;
    float thick;
    int rgb[3], distances[2];
} Args;

void usage()
{
    printf("Usage: main --eval <eval option> [args] \n");
    printf("-----------------------------------------------------------------------------------------------------\n");
    printf("Eval options: \n");
    printf("1: SIRS                  - Evaluate color homogeneity with SIRS measure. \n");
    printf("                           Optionally, one may write an image mapping superpixels to their SIRS' scores\n");
    printf("                           by providing an output path in \"--imgScores\" (one may set \"--drawScores\" \n");
    printf("                           to 1 in order to also write the scores); or write the reconstructed image \n");
    printf("                           with SIRS by providing an output path in \"-recon\". \n");
    printf("2: EV                    - Evaluate color homogeneity with EV measure. \n");
    printf("                           Optionally, one may write an image mapping superpixels to their EV' scores\n");
    printf("                           by providing an output path in \"--imgScores\" (one may set \"--drawScores\" \n");
    printf("                           to 1 in order to also write the scores); or write the reconstructed image \n");
    printf("                           with EV by providing an output path in \"-recon\". \n");
    printf("3: BR                    - Evaluate boundary aderence with BR measure. \n");
    printf("4: UE                    - Evaluate boundary aderence with UE measure. \n");
    printf("5: CO                    - Evaluate compactness with CO measure. \n");
    printf("6: Connectivity          - Evaluate the number of superpixels with distinct labels and the number of \n");
    printf("                           connected components. Optionally, one may enforce the number of connected \n");
    printf("                           components be equal to the number of labels providing an output path in \n");
    printf("                           \"--save\" option. \n");
    printf("7: Superpixels' number   - Evaluate the number of superpixels with distinct labels. \n");
    printf("                           Optionally, one may relabel the superpixels, with a distint label for each \n");
    printf("                           connected component by providing an output path in \"--save\" option. \n");
    printf("8: Qualitative           - Evaluate the number of superpixels with distinct labels. \n");
    printf("-----------------------------------------------------------------------------------------------------\n");
    printf("Arguments required for any evaluation option: \n");
    printf("--eval        - Superpixel evaluation option. Type: int. \n");
    printf("--label       - A pgm/png path with labeled superpixels image(s). Type: char* \n");
    printf("--ext         - File extension for image labels. Type: char* \n");
    printf("-----------------------------------------------------------------------------------------------------\n");
    printf("Arguments required for some evaluation options: \n");
    printf("--img         - Original image or gt file/path. Used in metrics 1,2,7,8 (original image), \n");
    printf("                and 3,4 (image ground-truth). Type: char* \n");
    printf("--k           - Desired number of superpixels. Used in metric 7. Type: int \n");
    printf("-----------------------------------------------------------------------------------------------------\n");
    printf("Arguments with default value: \n");
    printf("--buckets     - Used in metric 1. Default: 16. Type: int \n");
    printf("--alpha       - Used in metric 1. Default: 4. Type: int \n");
    printf("--gaussVar    - Used in metric 1. Default: 0.01. Type: double \n");
    printf("--rgb         - Used in metric 8. RGB color for superpixels' boundaries. \n");
    printf("                The color is a list with three float values in [0,1]. \n");
    printf("                Default: 1,0,0. Type: float[3]. \n");
    printf("--thick       - Used in metric 8. Thick of superpixels' boundaries. Default: 1. Type: int \n");
    printf("--distances   - Used in metric 8. Two values for x and y image distances, respectively. \n");
    printf("                Default: xsize,ysize. Type: int[2] \n");
    printf("--save        - Used in metrics 6, 7, and 8. Optional in 6 and 7. Directoy for output image. \n");
    printf("                Type: char* \n");
    printf("-----------------------------------------------------------------------------------------------------\n");
    printf("Optional arguments: \n");
    printf("--log         - txt path for th overall results (only used when --img is a directory). \n");
    printf("                Optional. Type: char* \n");
    printf("--dlog        - txt file path with evaluated results (for each image). Optional. Type: char* \n");
    printf("--imgScores   - Used in metrics 1 and 2. Optional. Path to save images whose color maps indicate \n");
    printf("                SIRS/EV scores. Type: char* \n");
    printf("--drawScores  - Used in metrics 1 and 2. Optional. Boolean option when using \"--imgScores\" to show \n");
    printf("                score values. Type: bool \n");
    printf("--recon       - Used in metrics 1 and 2. Optional. Path to save the reconstructed images. Type: char* \n");
    printf("--label2      - Used in metric 8. A pgm/png path with other labeled images. Type: char* \n");
    printf("-----------------------------------------------------------------------------------------------------\n");

    printError("main", "Too many/few parameters");
}

bool initArgs(Args *args, int argc, char *argv[])
{
    char *nbucketsChar = NULL, *alphaChar = NULL,
         *metricChar = NULL, *drawScoresChar = NULL,
         *gauss_varianceChar = NULL, *kChar = NULL,
         *tickChar = NULL, *rgbChar = NULL, *distancesChar = NULL,
         *removeColorChar = NULL, *removeSizeChar = NULL,
         *relabelSpsChar = NULL;

    args->img_path = parseArgs(argv, argc, "--img");
    args->label_path = parseArgs(argv, argc, "--label");
    args->label_path2 = parseArgs(argv, argc, "--label2");
    args->label_ext = parseArgs(argv, argc, "--ext");
    args->logFile = parseArgs(argv, argc, "--log");
    args->dLogFile = parseArgs(argv, argc, "--dlog");

    // Receives GT for filtering, methods 1,2,5,6,7
    args->gt_path = parseArgs(argv, argc, "--gt");

    args->imgScoresPath = parseArgs(argv, argc, "--imgScores");
    args->imgRecon = parseArgs(argv, argc, "--recon");
    args->saveLabels = parseArgs(argv, argc, "--save");
    kChar = parseArgs(argv, argc, "--k");
    nbucketsChar = parseArgs(argv, argc, "--buckets");
    alphaChar = parseArgs(argv, argc, "--alpha");
    metricChar = parseArgs(argv, argc, "--eval");
    drawScoresChar = parseArgs(argv, argc, "--drawScores");
    gauss_varianceChar = parseArgs(argv, argc, "--gaussVar");
    tickChar = parseArgs(argv, argc, "--thick");
    rgbChar = parseArgs(argv, argc, "--rgb");
    distancesChar = parseArgs(argv, argc, "--distances");

    // Parameters to filter superpixels
    removeColorChar = parseArgs(argv, argc, "--rmcolor");
    removeSizeChar = parseArgs(argv, argc, "--rmsize");
    relabelSpsChar = parseArgs(argv, argc, "--relabel");

    if (strcmp(args->saveLabels, "-") == 0)
        args->saveLabels = NULL;

    args->k = strcmp(kChar, "-") != 0 ? atoi(kChar) : 0;
    args->gauss_variance = strcmp(gauss_varianceChar, "-") != 0 ? atof(gauss_varianceChar) : 0.01;
    args->buckets = strcmp(nbucketsChar, "-") != 0 ? atoi(nbucketsChar) : 16;
    args->alpha = strcmp(alphaChar, "-") != 0 ? atoi(alphaChar) : 4;
    args->metric = strcmp(metricChar, "-") != 0 ? atoi(metricChar) : 1;
    args->drawScores = strcmp(drawScoresChar, "-") != 0 ? atoi(drawScoresChar) : false;
    args->gauss_variance = strcmp(gauss_varianceChar, "-") != 0 ? atof(gauss_varianceChar) : 0.01;
    args->thick = strcmp(tickChar, "-") != 0 ? atof(tickChar) : 1.0;

    args->removeColor = strcmp(removeColorChar, "-") != 0 ? atoi(removeColorChar) : -1;
    args->removeSize = strcmp(removeSizeChar, "-") != 0 ? atoi(removeSizeChar) : -1;
    args->recreateLabels = strcmp(relabelSpsChar, "-") != 0 ? atoi(relabelSpsChar) : -1;

    if (strcmp(args->logFile, "-") == 0)
        args->logFile = NULL;
    if (strcmp(args->dLogFile, "-") == 0)
        args->dLogFile = NULL;
    if (strcmp(args->imgScoresPath, "-") == 0)
        args->imgScoresPath = NULL;
    if (strcmp(args->imgRecon, "-") == 0)
        args->imgRecon = NULL;
    if (strcmp(args->label_path2, "-") == 0)
        args->label_path2 = NULL;
    if (strcmp(args->gt_path, "-") == 0)
        args->gt_path = NULL;

    if (strcmp(rgbChar, "-") != 0)
    {
        int i = 0;
        char *tok, *tmp;

        tmp = iftCopyString(rgbChar);
        tok = strtok(tmp, ",");
        while (tok != NULL && i < 3)
        {
            float c;
            c = atof(tok);
            if (c >= 0 && c <= 1)
                args->rgb[i] = c * 255;
            else
                iftError("The color should be within [0,1]", "initArgs");

            tok = strtok(NULL, ",");
            i++;
        }
        if ((tok != NULL && i == 3) || (tok == NULL && i < 2))
            iftError("Three colors are required for the RGB", "initArgs");
        free(tmp);
    }
    else
    {
        args->rgb[0] = 255;
        args->rgb[1] = 0;
        args->rgb[2] = 0;
    }

    if (strcmp(distancesChar, "-") != 0)
    {
        char *tok, *tmp;

        tmp = iftCopyString(distancesChar);
        tok = strtok(tmp, ",");

        int i = 0;
        while (tok != NULL && i < 2)
        {
            args->distances[i] = atoi(tok);
            tok = strtok(NULL, ",");
            i++;
        }

        if ((tok != NULL && i == 2) || (tok == NULL && i < 2))
            iftError("Two integer values are required for x and y distances (respectively) in format x,y", "initArgs");
        free(tmp);
    }
    else
    {
        args->distances[0] = -1;
        args->distances[1] = -1;
    }

    if (args->metric > 10 || args->metric < 1 || strcmp(args->label_path, "-") == 0 || strcmp(args->label_ext, "-") == 0)
        return false;
    if (args->metric == 1 && strcmp(args->img_path, "-") == 0 && (args->buckets < 1 || args->alpha < 1 || args->alpha > args->buckets * 7))
        return false;
    if ((args->metric == 2 || args->metric == 3 || args->metric == 4) && strcmp(args->img_path, "-") == 0)
        return false;
    if (args->metric == 7 && args->k < 1)
        return false;
    if (args->metric == 8 && (strcmp(args->img_path, "-") == 0 || strcmp(args->saveLabels, "-") == 0))
        return false;
    if (args->metric == 9 && (strcmp(args->label_path, "-") == 0 || strcmp(args->img_path, "-") == 0))
        return false;
    if (args->metric == 10 && (strcmp(args->label_path, "-") == 0 || strcmp(args->img_path, "-") == 0))
        return false;

    return true;
}

//==========================================================

int getNumSuperpixels(iftImage *L)
{
    int maxLabel = 0;
    for (int i = 0; i < L->n; i++)
    {
        if (L->val[i] > maxLabel)
            maxLabel = L->val[i];
    }
    maxLabel++;

    int tmp[iftMax(L->n, maxLabel)], numLabels = 0;

    for (int i = 0; i < iftMax(L->n, maxLabel); i++)
        tmp[i] = 0;
    for (int i = 0; i < L->n; i++)
    {
        if (L->val[i] >= 0)
            tmp[L->val[i]] = -1;
    }
    for (int i = 0; i < iftMax(L->n, maxLabel); i++)
    {
        if (tmp[i] == -1)
            numLabels++;
    }

#ifdef DEBUG
    printf("Number of superpixels: %d\n", numLabels);
#endif

    return numLabels;
}

double *getImageVariance_channels(iftImage *image, iftImage *labels)
{
    int num_channels = iftIsColorImage(image) ? 3 : 1;
    double acum_color[num_channels];
    double *variance;
    int num_ignored_pixels = 0;

    variance = (double *)calloc(num_channels, sizeof(double));

    for (int c = 0; c < num_channels; c++)
        acum_color[c] = 0.0;

    for (int i = 0; i < image->n; i++)
    {
        if(labels->val[i] < 0)
        {
            num_ignored_pixels++;
            continue;
        }

        acum_color[0] += (double)image->val[i] / 255.0;
        if (num_channels > 1)
            acum_color[1] += (double)image->Cb[i] / 255.0;
        if (num_channels > 2)
            acum_color[2] += (double)image->Cr[i] / 255.0;
    }

    for (int c = 0; c < num_channels; c++)
        acum_color[c] /= (double)(image->n-num_ignored_pixels);

    for (int i = 0; i < image->n; i++)
    {
        variance[0] += ((double)image->val[i] / 255.0 - acum_color[0]) * ((double)image->val[i] / 255.0 - acum_color[0]);
        if (num_channels > 1)
            variance[1] += ((double)image->Cb[i] / 255.0 - acum_color[1]) * ((double)image->Cb[i] / 255.0 - acum_color[1]);
        if (num_channels > 2)
            variance[2] += ((double)image->Cr[i] / 255.0 - acum_color[2]) * ((double)image->Cr[i] / 255.0 - acum_color[2]);
    }

    for (int c = 0; c < num_channels; c++)
        variance[c] /= (double)(image->n-num_ignored_pixels);
    return variance;
}

// return the number of connected components and change labels to have a unique label for each connected component
int relabelSuperpixels(iftImage *labels, int connectivity)
{
    PrioQueue *queue;
    NodeAdj *adj_rel;
    double *visited;

    // mark which pixels were touched by a neighbor with the same label
    visited = (double *)calloc(labels->n, sizeof(double));
    queue = createPrioQueue(labels->n, visited, MINVAL_POLICY);
    adj_rel = create8NeighAdj();

    // set initial label
    int label = labels->val[0], i = 0;
    int newLabel = -1;
    visited[i] = -2;
    insertPrioQueue(&queue, i);

    while (!isPrioQueueEmpty(queue))
    {
        int vertex = popPrioQueue(&queue);
        int vertexLabel = labels->val[vertex];
        NodeCoords vertexCoords;
        vertexCoords = getNodeCoordsImage(labels->xsize, vertex);

        // ignore label -1
        if (vertexLabel > -1)
        {
            // if the spx wasn't touched by a neighbor
            // with the same label, set a new label for it
            if (visited[vertex] == 0 || newLabel < 0)
            {
                newLabel++; // set new label
                label = vertexLabel; // set old label
            }
            labels->val[vertex] = newLabel;
        }

        for (int i = 0; i < adj_rel->size; i++)
        {
            NodeCoords adjCoords;
            adjCoords = getAdjacentNodeCoords(adj_rel, vertexCoords, i);
            if (areValidNodeCoordsImage(labels->ysize, labels->xsize, adjCoords))
            {
                int adjVertex = getNodeIndexImage(labels->xsize, adjCoords);
                if (queue->state[adjVertex] != BLACK_STATE)
                {
                    if (labels->val[adjVertex] == label && vertexLabel != -1)
                        visited[adjVertex] = -1;
                    if (queue->state[adjVertex] == WHITE_STATE)
                        insertPrioQueue(&queue, adjVertex);
                    else
                    {
                        if (labels->val[adjVertex] == label)
                            moveIndexUpPrioQueue(&queue, adjVertex);
                    }
                }
            }
        }
    }
    free(visited);
    freePrioQueue(&queue);
    freeNodeAdj(&adj_rel);
    return getNumSuperpixels(labels);
}

// warning: this function is not able to deal with negative labels
int enforceNumSuperpixel(iftImage *labels, iftImage *image, int numDesiredSpx)
{
    NodeAdj *adj_rel;
    PrioQueue *queue;
    int num_channels;

    if (iftIsColorImage(image))
        num_channels = 3;
    else
        num_channels = 1;

    adj_rel = create8NeighAdj();
    int numSpx = relabelSuperpixels(labels, 8); // ensure connectivity

    float meanColor[numSpx][3];
    bool **neighbors = (bool **)malloc(numSpx * sizeof(bool *));
    double *sizeSpx = (double *)calloc(numSpx, sizeof(double));
    int new_labels[numSpx];

    for (int i = 0; i < numSpx; i++)
    {
        meanColor[i][0] = meanColor[i][1] = meanColor[i][2] = 0;
        neighbors[i] = (bool *)calloc(numSpx, sizeof(bool));
        new_labels[i] = i;
    }

    // for each superpixel find its adjacent superpixels and compute the mean color and size
    for (int node = 0; node < labels->n; node++)
    {
        NodeCoords coords;
        int label = labels->val[node];

        coords = getNodeCoordsImage(labels->xsize, node);

        sizeSpx[label] += 1;
        meanColor[label][0] += (float)image->val[node];

        if (num_channels > 1)
            meanColor[label][1] += (float)image->Cb[node];
        else
            meanColor[label][1] += (float)image->val[node];

        if (num_channels > 2)
            meanColor[label][2] += (float)image->Cr[node];
        else
            meanColor[label][2] += (float)image->val[node];

        for (int j = 0; j < adj_rel->size; j++)
        {
            NodeCoords adjCoords = getAdjacentNodeCoords(adj_rel, coords, j);
            if (areValidNodeCoordsImage(labels->ysize, labels->xsize, adjCoords))
            {
                int adjLabel = labels->val[getNodeIndexImage(labels->xsize, adjCoords)];
                if (adjLabel != label)
                {
                    if (neighbors[label][adjLabel] == false)
                    {
                        neighbors[label][adjLabel] = true;
                        neighbors[adjLabel][label] = true;
                    }
                }
            }
        }
    }

    queue = createPrioQueue(numSpx, sizeSpx, MINVAL_POLICY);
    for (int i = 0; i < numSpx; i++)
        insertPrioQueue(&queue, i);

    freeNodeAdj(&adj_rel);

    int k = numSpx - numDesiredSpx; // number of superpixels to merge with other

    // for each superpixel in ascending order of size, merge it with the most similar adjacent superpixel
    while (k > 0 && !isPrioQueueEmpty(queue))
    {
        int label = popPrioQueue(&queue);
        float minDistance = INFINITY;
        float localMean[3];
        int new_label = -1;

        int parent = new_labels[label];
        while (label != parent)
        {
            label = parent;
            parent = new_labels[label];
        }

        localMean[0] = meanColor[label][0] / (float)sizeSpx[label];
        localMean[1] = meanColor[label][1] / (float)sizeSpx[label];
        localMean[2] = meanColor[label][2] / (float)sizeSpx[label];

        bool *ptr_adj = neighbors[label];
        for (int adj = 0; adj < numSpx; adj++, ptr_adj++)
        {
            if (*(ptr_adj) == false)
                continue;

            float adjMean[3];
            int adjLabel = *(ptr_adj);
            parent = new_labels[adjLabel];

            while (adjLabel != parent)
            {
                adjLabel = parent;
                parent = new_labels[adjLabel];
            }

            if (adjLabel != label)
            {
                adjMean[0] = meanColor[adjLabel][0] / (float)sizeSpx[adjLabel];
                adjMean[1] = meanColor[adjLabel][1] / (float)sizeSpx[adjLabel];
                adjMean[2] = meanColor[adjLabel][2] / (float)sizeSpx[adjLabel];

                float distance = (localMean[0] - adjMean[0]) * (localMean[0] - adjMean[0]) +
                                 (localMean[1] - adjMean[1]) * (localMean[1] - adjMean[1]) +
                                 (localMean[2] - adjMean[2]) * (localMean[2] - adjMean[2]);

                if (distance < minDistance)
                {
                    minDistance = distance;
                    new_label = adjLabel;
                }
            }
            else
            {
                *(ptr_adj) = false;
                neighbors[adj][label] = false;
            }
        }

        // merge with the most similar neighbor superpixel with different label
        if (new_label != -1)
        {
            new_labels[label] = new_label;

            meanColor[new_label][0] += meanColor[label][0];
            meanColor[new_label][1] += meanColor[label][1];
            meanColor[new_label][2] += meanColor[label][2];
            sizeSpx[new_label] += sizeSpx[label];

            neighbors[new_label][label] = false;
            neighbors[label][new_label] = false;
            
            bool *ptr_adj = neighbors[label];
            bool *ptr_new = neighbors[new_label];
            for (int adj = 0; adj < numSpx; adj++, ptr_adj++, ptr_new++)
            {
                if (adj != label && adj != new_label && *ptr_adj)
                    (*ptr_new) = true;
            }
            moveIndexDownPrioQueue(&queue, new_label);
            k--;
        }
    }

    for (k = 0; k < numSpx; k++)
    {
        free(neighbors[k]);
    }
    free(neighbors);
    free(sizeSpx);

    // atualiza os rótulos que foram unidos - update labels that were merged
    for (k = 0; k < numSpx; k++)
    {
        int label;
        int parent;
        label = k;
        parent = new_labels[label];

        while (label != parent)
        {
            label = parent;
            parent = new_labels[label];
            new_labels[label] = parent;
        }
        label = k;
        while (label != parent)
        {
            int tmp = new_labels[label];
            new_labels[label] = parent;
            label = tmp;
        }
    }

    // rotula os pixels com os rótulos novos - labels superpixels with new labels
    for (int i = 0; i < labels->n; i++)
        labels->val[i] = new_labels[labels->val[i]];

    return relabelSuperpixels(labels, 8);
}

//==========================================================

// compute RBD descriptor for a superpixel
void RBD(iftImage *image, iftImage *labels, int label, int nbuckets, int *alpha, float **Descriptor)
{
    /* Compute the superpixels descriptors
        image : RGB image
        labels     : Image labels (0,K-1)
        Descriptor : Descriptor[num_channels][alpha]
    */

    PrioQueue *queue;
    int num_histograms;
    int superpixel_size = 0, num_channels;

    if (iftIsColorImage(image)) num_channels = 3;
    else num_channels = 1;

    num_histograms = pow(2, num_channels) - 1;
    long int ColorHistogram[num_histograms][nbuckets][3]; // Descriptor[image->num_channels][nbuckets]
    double V[num_histograms * nbuckets];                  // buckets priority : V[image->num_channels][nbuckets]

    queue = createPrioQueue(num_histograms * nbuckets, V, MINVAL_POLICY);

    for (int h = 0; h < num_histograms; h++)
    {
        for (int b = 0; b < nbuckets; b++)
        {
            V[h * nbuckets + b] = 0;
            for (int c = 0; c < num_channels; c++)
                ColorHistogram[h][b][c] = 0;
        }
    }

#ifdef DEBUG
    printf("RBD: Compute histogram\n");
#endif

    // compute histograms
    for (int i = 0; i < image->n; i++)
    {
        if (labels->val[i] == label)
        {
            superpixel_size++;
            int hist_id = 0;
            int bit = 1;

            if (image->val[i] < image->Cb[i])
                hist_id = hist_id | bit;
            else
            {
                if (image->val[i] < image->Cr[i])
                    hist_id = hist_id | bit;
            }
            bit = bit << 1;

            if (image->Cb[i] < image->val[i])
                hist_id = hist_id | bit;
            else
            {
                if (image->Cb[i] < image->Cr[i])
                    hist_id = hist_id | bit;
            }
            bit = bit << 1;

            if (image->Cr[i] < image->val[i])
                hist_id = hist_id | bit;
            else
            {
                if (image->Cr[i] < image->Cb[i])
                    hist_id = hist_id | bit;
            }
            bit = bit << 1;

            hist_id = ~hist_id;
            hist_id *= -1;

            // find one max channel index
            int max_channel = 0;
            int tmp_hist_id = hist_id;

            while (tmp_hist_id % 2 == 0)
            {
                tmp_hist_id = tmp_hist_id >> 1;
                max_channel++;
            }

            int bin;
            if (max_channel == 0)
                bin = floor(((float)image->val[i] / 255.0) * nbuckets);
            else
            {
                if (max_channel == 1)
                    bin = floor(((float)image->Cb[i] / 255.0) * nbuckets);
                else
                    bin = floor(((float)image->Cr[i] / 255.0) * nbuckets);
            }
            hist_id--;

            if (hist_id < 0 || hist_id > 6)
                printf(">> hist_id:%d \n", hist_id);
            if (bin < 0 || bin > nbuckets)
                printf(">> bin:%d \n", bin);
            if (hist_id * nbuckets + bin < 0 || hist_id * nbuckets + bin > num_histograms * nbuckets)
                printf(">> hist_id * nbuckets + bin:%d \n", hist_id * nbuckets + bin);

            V[hist_id * nbuckets + bin]++;
            ColorHistogram[hist_id][bin][0] += (long int)image->val[i];
            ColorHistogram[hist_id][bin][1] += (long int)image->Cb[i];
            ColorHistogram[hist_id][bin][2] += (long int)image->Cr[i];
        }
    }

#ifdef DEBUG
    printf("getSuperpixelDescriptor: Find the higher alpha buckets\n");
#endif

    for (int c = 0; c < num_histograms; c++)
    {
        for (int b = 0; b < nbuckets; b++)
        {
            if (V[c * nbuckets + b] > 0)
            {
                if (isPrioQueueEmpty(queue) || queue->last_elem_pos < (*alpha) - 1)
                    insertPrioQueue(&queue, c * nbuckets + b); // push (color, frequency) into Q, sorted by V[i]
                else
                {
                    if (!isPrioQueueEmpty(queue) && V[queue->node[0]] < V[c * nbuckets + b])
                    {
                        popPrioQueue(&queue);
                        insertPrioQueue(&queue, c * nbuckets + b); // push (color, frequency) into Q, sorted by V[i]
                    }
                }
            }
        }
    }

    // Get the higher alpha buckets
    if (isPrioQueueEmpty(queue))
    {
        (*alpha) = 0;
        int a = 0;
        for (int c = 0; c < num_channels; c++)
            Descriptor[a][c] = 0;
    }
    else
    {
        if (queue->last_elem_pos < (*alpha) - 1)
            (*alpha) = queue->last_elem_pos + 1;

        for (int a = (*alpha) - 1; a >= 0; a--)
        {
            int val = popPrioQueue(&queue);
            int bin = val % nbuckets;
            int hist = (val - bin) / nbuckets;

            for (int c = 0; c < num_channels; c++)
            {
                Descriptor[a][c] = (float)ColorHistogram[hist][bin][c] / (float)V[val]; // get the mean color
            }
        }
    }

#ifdef DEBUG
    printf("\nDESCRIPTOR: \n");
    for (int a = 0; a < (*alpha); a++)
    {
        printf("\t alpha %d (", a);
        for (int c = 0; c < num_channels; c++)
        {
            printf("%f ", Descriptor[a][c]);
        }
        printf(") \n\n");
    }
#endif

#ifdef DEBUG
    printf("\nFREE STRUCTURES: getSuperpixelDescriptor\n");
#endif

    freePrioQueue(&queue);
}

//==========================================================
// COLOR HOMOGENEITY MEASURES
//==========================================================

double *SIRS(iftImage *labels, iftImage *image, 
            int alpha, int nbuckets, char *reconFile, double gauss_variance, 
            double *score)
{
    double *histogramVariation;
    float ***Descriptor;  // Descriptor[numSup][alpha][num_channels]
    int *descriptor_size; // descriptor_size[numSup]
    int emptySuperpixels;
    double **MSE;
    iftImage *recons;
    double **mean_buckets;         // mean_buckets[superpixels][num_channels];
    double **variation_descriptor; // variation_descriptor[superpixels][num_channels];
    int num_channels;

    if (!iftIsColorImage(image))
        iftError("The original image must be color or 3-channel grayscale", "SIRS");
    if (iftIsColorImage(labels))
        iftError("The label image must be 1-channel grayscale", "SIRS");

    num_channels = 3;
    (*score) = 0;
    emptySuperpixels = 0;

    if (reconFile != NULL)
        recons = iftCreateColorImage(image->xsize, image->ysize, 1, 8); // for RGB colors, depth = 8

    // get the higher label = number of superpixels
    int superpixels = 0;
    for (int i = 0; i < image->n; ++i)
    {
        if (labels->val[i] > superpixels)
            superpixels = labels->val[i];
    }
    superpixels++;

    histogramVariation = (double *)calloc(superpixels, sizeof(double));
    descriptor_size = (int *)calloc(superpixels, sizeof(int));
    Descriptor = (float ***)calloc(superpixels, sizeof(float **));
    MSE = (double **)calloc(superpixels, sizeof(double *));
    mean_buckets = (double **)calloc(superpixels, sizeof(double *));
    variation_descriptor = (double **)calloc(superpixels, sizeof(double *));

    int *superpixelSize = (int *)calloc(superpixels, sizeof(int));
    
    // compute superpixels area
    for (int i = 0; i < image->n; ++i)
    {
        if (labels->val[i] != -1) superpixelSize[labels->val[i]]++;
    }

    for (int s = 0; s < superpixels; s++)
    {
        if (superpixelSize[s] == 0){
            emptySuperpixels++;
            continue;
        }   

#ifdef DEBUG
        printf("SUPERPIXEL %d: \n", s);
#endif
        descriptor_size[s] = alpha;
        Descriptor[s] = (float **)calloc(alpha, sizeof(float *));
        MSE[s] = (double *)calloc(num_channels, sizeof(double));
        mean_buckets[s] = (double *)calloc(num_channels, sizeof(double));
        variation_descriptor[s] = (double *)calloc(num_channels, sizeof(double));

        for (int c = 0; c < alpha; c++)
            Descriptor[s][c] = (float *)calloc(num_channels, sizeof(float));

#ifdef DEBUG
        printf("call RBD \n");
#endif
        RBD(image, labels, s, nbuckets, &(descriptor_size[s]), Descriptor[s]);

        for (int i = 0; i < num_channels; i++)
            MSE[s][i] = 0.0;

        for (int a = 0; a < descriptor_size[s]; a++)
        {
            for (int c = 0; c < num_channels; c++)
                mean_buckets[s][c] += ((double)Descriptor[s][a][c] / 255.0);
        }
        for (int c = 0; c < num_channels; c++)
            mean_buckets[s][c] /= descriptor_size[s];

        for (int a = 0; a < descriptor_size[s]; a++)
        {
            for (int c = 0; c < num_channels; c++)
                variation_descriptor[s][c] = iftMax(variation_descriptor[s][c], abs(((double)Descriptor[s][a][c] / 255.0) - mean_buckets[s][c]));
        }
    }

#ifdef DEBUG
    printf("compute variation \n");
#endif
    iftColor RGB, YCbCr;
    int ignoredPixels = 0;

    for (int i = 0; i < image->n; i++)
    {
        int label;
        double minVariance;
        int descIndex;

        label = labels->val[i];
        descIndex = 0;
        minVariance = 0;

        if (label != -1)
        {
            minVariance += ((double)image->val[i] / 255.0 - (double)Descriptor[label][0][0] / 255.0) * ((double)image->val[i] / 255.0 - (double)Descriptor[label][0][0] / 255.0);
            minVariance += ((double)image->Cb[i] / 255.0 - (double)Descriptor[label][0][1] / 255.0) * ((double)image->Cb[i] / 255.0 - (double)Descriptor[label][0][1] / 255.0);
            minVariance += ((double)image->Cr[i] / 255.0 - (double)Descriptor[label][0][2] / 255.0) * ((double)image->Cr[i] / 255.0 - (double)Descriptor[label][0][2] / 255.0);

            // find the most distance descriptor values
            for (int h1 = 1; h1 < descriptor_size[label]; h1++)
            {
                double val = 0;

                val += ((double)image->val[i] / 255.0 - (double)Descriptor[label][h1][0] / 255.0) * ((double)image->val[i] / 255.0 - (double)Descriptor[label][h1][0] / 255.0);
                val += ((double)image->Cb[i] / 255.0 - (double)Descriptor[label][h1][1] / 255.0) * ((double)image->Cb[i] / 255.0 - (double)Descriptor[label][h1][1] / 255.0);
                val += ((double)image->Cr[i] / 255.0 - (double)Descriptor[label][h1][2] / 255.0) * ((double)image->Cr[i] / 255.0 - (double)Descriptor[label][h1][2] / 255.0);

                if (val < minVariance)
                {
                    minVariance = val;
                    descIndex = h1;
                }
            }

            if (reconFile != NULL)
            {
                for (int c = 0; c < num_channels; c++)
                {
                    RGB.val[c] = (int)Descriptor[label][descIndex][c];
                }
                YCbCr = iftRGBtoYCbCr(RGB, 255);
                recons->val[i] = YCbCr.val[0];
                recons->Cb[i] = YCbCr.val[1];
                recons->Cr[i] = YCbCr.val[2];
            }
            MSE[label][0] += pow(abs((double)image->val[i] / 255.0 - (double)Descriptor[label][descIndex][0] / 255.0), iftMax(2 - variation_descriptor[label][0], 0.001));
            MSE[label][1] += pow(abs((double)image->Cb[i] / 255.0 - (double)Descriptor[label][descIndex][1] / 255.0), iftMax(2 - variation_descriptor[label][1], 0.001));
            MSE[label][2] += pow(abs((double)image->Cr[i] / 255.0 - (double)Descriptor[label][descIndex][2] / 255.0), iftMax(2 - variation_descriptor[label][2], 0.001));
        }else ignoredPixels++;
    }

    double *variance = getImageVariance_channels(image, labels);
    double sum_MSE[num_channels];

    for (int c = 0; c < num_channels; c++)
        sum_MSE[c] = 0.0;

#ifdef DEBUG
    printf("variance: ");
    for (int c = 0; c < num_channels; c++)
        printf("%f ", variance[c]);
    printf("\n");
    printf("\n\nScores:\n");
#endif
    for (int s = 0; s < superpixels; s++)
    {
        histogramVariation[s] = 0;
        if (superpixelSize[s] > 0)
        {
            for (int c = 0; c < num_channels; c++)
            {
                histogramVariation[s] += exp(-(MSE[s][c] / superpixelSize[s]) / gauss_variance);
                sum_MSE[c] += MSE[s][c];
            }
        }
        histogramVariation[s] /= num_channels;

#ifdef DEBUG
        printf("%f + ", histogramVariation[s]);
#endif
    }

    (*score) = 0;
    for (int c = 0; c < num_channels; c++)
    {
        (*score) += sum_MSE[c];
    }
    (*score) /= num_channels;
    (*score) = exp(-((*score) / (image->n - ignoredPixels)) / (gauss_variance));

#ifdef DEBUG
    printf("= %f \n", (*score));
#endif

    for (int s = 0; s < superpixels; s++)
    {
        if(superpixelSize[s] > 0){
            for (int a = 0; a < alpha; a++)
                free(Descriptor[s][a]);
            free(Descriptor[s]);
            free(MSE[s]);
            free(mean_buckets[s]);
            free(variation_descriptor[s]);
        }
    }

    free(descriptor_size);
    free(Descriptor);
    free(MSE);
    free(mean_buckets);
    free(variation_descriptor);
    free(superpixelSize);
    free(variance);

    if (reconFile != NULL)
    {
        iftWriteImageByExt(recons, reconFile);
        iftDestroyImage(&recons);
    }

    return histogramVariation;
}

double *computeExplainedVariation(iftImage *labels, iftImage *image, char *reconFile, double *score)
{
    (*score) = 0;

    double *supExplainedVariation;
    double *valuesTop, *valuesBottom;
    iftImage *recons;
    int num_channels, numIgnoredSuperpixels = 0, numIgnoredPixels = 0;

    if (!iftIsColorImage(image))
        iftError("The original image must be color or 3-channel grayscale", __func__);
    if (iftIsColorImage(labels))
        iftError("The label image must be 1-channel grayscale", __func__);

    num_channels = 3;

    // get the greater label
    int superpixels = 0;
    for (int i = 0; i < image->n; ++i)
    {
        if (labels->val[i] > superpixels)
            superpixels = labels->val[i];
    }
    superpixels++;

    if (reconFile != NULL)
        recons = iftCreateColorImage(image->xsize, image->ysize, 1, 8); // for RGB colors, depth = 8

    supExplainedVariation = (double *)calloc(superpixels, sizeof(double));
    valuesTop = (double *)calloc(superpixels, sizeof(double));
    valuesBottom = (double *)calloc(superpixels, sizeof(double));

    float **mean = (float **)malloc(superpixels * sizeof(float *));
    int *count = (int *)calloc(superpixels, sizeof(int));
    for (int i = 0; i < superpixels; i++)
        mean[i] = (float *)calloc(3, sizeof(float));

    float *overall_mean = (float *)calloc(3, sizeof(float));
    for (int i = 0; i < image->n; ++i)
    {
        if (labels->val[i] > -1)
        {
            mean[labels->val[i]][0] += image->val[i];
            overall_mean[0] += image->val[i];

            mean[labels->val[i]][1] += image->Cb[i];
            overall_mean[1] += image->Cb[i];

            mean[labels->val[i]][2] += image->Cr[i];
            overall_mean[2] += image->Cr[i];

            count[labels->val[i]]++;
        }else 
            numIgnoredPixels++;
    }

    for (int i = 0; i < superpixels; ++i)
    {
        if (count[i] > 0)
        {
            for (int c = 0; c < num_channels; ++c)
                mean[i][c] /= count[i];
        }else 
            numIgnoredSuperpixels++;
    }
    
    overall_mean[0] /= (image->n-numIgnoredPixels);
    overall_mean[1] /= (image->n-numIgnoredPixels);
    overall_mean[2] /= (image->n-numIgnoredPixels);

    int color_in[num_channels];
    int color_out[num_channels];
    iftColor YCbCr, RGB;
    for (int i = 0; i < image->n; ++i)
    {
        int label = labels->val[i];
        if (labels->val[i] != -1)
        {
            valuesTop[label] += (mean[label][0] - overall_mean[0]) * (mean[labels->val[i]][0] - overall_mean[0]);
            valuesBottom[label] += (image->val[i] - overall_mean[0]) * (image->val[i] - overall_mean[0]);

            valuesTop[label] += (mean[label][1] - overall_mean[1]) * (mean[labels->val[i]][1] - overall_mean[1]);
            valuesBottom[label] += (image->Cb[i] - overall_mean[1]) * (image->Cb[i] - overall_mean[1]);

            valuesTop[label] += (mean[label][2] - overall_mean[2]) * (mean[labels->val[i]][2] - overall_mean[2]);
            valuesBottom[label] += (image->Cr[i] - overall_mean[2]) * (image->Cr[i] - overall_mean[2]);

            if (reconFile != NULL)
            {
                RGB.val[0] = mean[label][0];
                RGB.val[1] = mean[label][1];
                RGB.val[2] = mean[label][2];
                YCbCr = iftRGBtoYCbCr(RGB, 255);
                recons->val[i] = YCbCr.val[0];
                recons->Cb[i] = YCbCr.val[1];
                recons->Cr[i] = YCbCr.val[2];
            }
        }
    }

    free(mean);

    double sum_top = 0;
    double sum_bottom = 0;

#ifdef DEBUG
    printf("\n\nScores:\n");
#endif
    for (int s = 0; s < superpixels; s++)
    {
        if (count[s] > 0)
        {
            sum_top += valuesTop[s];
            sum_bottom += valuesBottom[s];

            if (valuesBottom[s] == 0)
                supExplainedVariation[s] = 1;
            else
                supExplainedVariation[s] = valuesTop[s] / valuesBottom[s];
#ifdef DEBUG
            printf("%f + ", supExplainedVariation[s]);
#endif
        }
    }

    free(count);
    (*score) += sum_top / sum_bottom;

#ifdef DEBUG
    printf("= %f \n", (*score));
#endif

    free(valuesTop);
    free(valuesBottom);

    if (reconFile != NULL)
        iftWriteImageByExt(recons, reconFile);

    return supExplainedVariation;
}

//==========================================================
// DELINEATION MEASURES
//==========================================================

bool hasSameColor(iftColor color, iftImage *img, int index){

    bool eq = iftIsColorImage(img) ? 
                color.val[1] == img->Cb[index] && color.val[2] == img->Cr[index] : 
                true;
    if (color.val[0] == img->val[index] && eq) return true;
    return false;
} 

bool is4ConnectedBoundaryPixel(iftImage *img, int y, int x, 
                            iftImage *labels)
{
    iftColor label_tmp;
    label_tmp.val[0] = iftImgVal(img, x, y, 0);
    label_tmp.val[1] = iftIsColorImage(img) ? iftImgCb(img, x, y, 0): 0;
    label_tmp.val[2] = iftIsColorImage(img) ? iftImgCr(img, x, y, 0): 0;

    if (y > 0 && 
        !hasSameColor(label_tmp, img, getIndexImage(img->xsize, y - 1, x)) &&
        iftImgVal(labels, x, y-1, 0) > -1)
        return true;
    if (x < img->xsize - 1 && 
        !hasSameColor(label_tmp, img, getIndexImage(img->xsize, y, x + 1)) &&
        iftImgVal(labels, x+1, y, 0) > -1)
        return true;
    if (x > 0 && 
        !hasSameColor(label_tmp, img, getIndexImage(img->xsize, y, x - 1)) &&
        iftImgVal(labels, x-1, y, 0) > -1)
        return true;
    if (y < img->ysize - 1 &&
        !hasSameColor(label_tmp, img, getIndexImage(img->xsize, y + 1, x)) &&
        iftImgVal(labels, x, y+1, 0) > -1)
        return true;

    return false;
}

void computeIntersectionMatrix(iftImage *labels, iftImage *gt,
                               int **intersection_matrix, int *superpixel_sizes,
                               int *gt_sizes, int cols, int rows)
{
    for (int i = 0; i < rows; ++i)
    {
        for (int j = 0; j < cols; ++j)
            intersection_matrix[i][j] = 0;
    }

    for (int i = 0; i < gt->ysize; ++i)
    {
        for (int j = 0; j < gt->xsize; ++j)
        {
            int index = getIndexImage(gt->xsize, i, j);

            if (labels->val[index] > -1)
            {
                intersection_matrix[gt->val[index]][labels->val[index]]++;
                superpixel_sizes[labels->val[index]]++;
                gt_sizes[gt->val[index]]++;
            }
        }
    }
}

double computeBoundaryRecall(iftImage *labels, iftImage *gt, float d)
{
    int H = gt->ysize; // num_rows
    int W = gt->xsize; // num_cols

    int r = round(d * sqrt(H * H + W * W));

    float tp = 0;
    float fn = 0;

    for (int i = 0; i < H; i++)
    {
        for (int j = 0; j < W; j++)
        {
            // Computes only if the superpixel in that position was not filtered out
            if (is4ConnectedBoundaryPixel(gt, i, j, labels) && 
                iftImgVal(labels, j, i, 0) > -1)
            {
                bool pos = false;
                for (int k = max(0, i - r); k < min(H - 1, i + r) + 1; k++)
                {
                    for (int l = max(0, j - r); l < min(W - 1, j + r) + 1; l++)
                    {
                        if (is4ConnectedBoundaryPixel(labels, k, l, labels))
                            pos = true;
                    }
                }

                if (pos)
                    tp++;
                else
                    fn++;
            }
                
        }
    }

    if (tp + fn > 0)
        return tp / (tp + fn);

    return 0;
}

double computeUndersegmentationError(iftImage *labels, iftImage *gt)
{
    int N = gt->xsize * gt->ysize; // May send N to the intersection matrix and change deduct its value from there
    int **intersection_matrix;
    int *superpixel_sizes;
    int *gt_sizes;

    int superpixels = 0;
    int gt_segments = 0;
    for (int i = 0; i < gt->n; ++i)
    {
        if (labels->val[i] > superpixels)
            superpixels = labels->val[i];
        if (gt->val[i] > gt_segments)
            gt_segments = gt->val[i];
        if(labels->val[i] < 0)
            N--;
    }
    superpixels++;
    gt_segments++;

    superpixel_sizes = (int *)calloc(superpixels, sizeof(int));
    gt_sizes = (int *)calloc(gt_segments, sizeof(int));

    intersection_matrix = (int **)malloc(gt_segments * sizeof(int *));
    for (int i = 0; i < gt_segments; ++i)
        intersection_matrix[i] = (int *)calloc(superpixels, sizeof(int));

    computeIntersectionMatrix(labels, gt, intersection_matrix, superpixel_sizes, gt_sizes, superpixels, gt_segments);

    free(gt_sizes);

    double error = 0;
    for (int j = 0; j < superpixels; ++j)
    {
        // Computes only super pixels that were not filtered
        if (superpixel_sizes[j] > 0)
        {
            int min = 0;
            bool init = true;
            for (int i = 0; i < gt_segments; ++i)
            {
                if(gt_sizes[i] > 0){ // ignore gt regions with filtered superpixels
                    int superpixel_j_minus_gt_i = superpixel_sizes[j] - intersection_matrix[i][j];
                    if (superpixel_j_minus_gt_i < 0)
                    {
                        printf("Invalid intersection computed, set difference is negative! \n");
                        return -1;
                    }
                    if (init || superpixel_j_minus_gt_i < min){
                        min = superpixel_j_minus_gt_i;
                        init = false;
                    }
                }    
            }
            error += min;
        }
    }

    free(superpixel_sizes);
    for (int i = 0; i < gt_segments; ++i)
        free(intersection_matrix[i]);
    free(intersection_matrix);
    return error / N;
}

double computeCompactness(iftImage *labels)
{
    int superpixels = 0;
    int num_filtered_pixels = 0;

    for (int i = 0; i < labels->ysize; ++i)
    {
        for (int j = 0; j < labels->xsize; ++j)
        {
            int label = iftImgVal(labels, j, i, 0);
            if (label > superpixels)
                superpixels = label;
        }
    }
    superpixels++;

    float *perimeter = (float *)calloc(superpixels, sizeof(float));
    float *area = (float *)calloc(superpixels, sizeof(float));

    for (int i = 0; i < labels->ysize; i++)
    {
        for (int j = 0; j < labels->xsize; j++)
        {
            int count = 0;
            int label = iftImgVal(labels, j, i, 0);

            if (label != -1)
            {
                if (i > 0)
                {
                    if (label != iftImgVal(labels, j, i - 1, 0))
                        count++;
                }
                else count++;

                if (i < labels->ysize - 1)
                {
                    if (label != iftImgVal(labels, j, i + 1, 0))
                        count++;
                }
                else count++;

                if (j > 0)
                {
                    if (label != iftImgVal(labels, j - 1, i, 0))
                        count++;
                }
                else count++;

                if (j < labels->xsize - 1)
                {
                    if (label != iftImgVal(labels, j + 1, i, 0))
                        count++;
                }
                else count++;

                perimeter[label] += count;
                area[label] += 1;
            }
            else num_filtered_pixels++;
        }
    }

    float compactness = 0;
    for (int i = 0; i < superpixels; ++i)
    {
        if (perimeter[i] > 0 && area[i] > 0)
            compactness += area[i] * (4 * IFT_PI * area[i]) / (perimeter[i] * perimeter[i]);
    }

    free(perimeter);
    free(area);
    compactness /= (labels->n - num_filtered_pixels);

    return compactness;
}

int *computeSuperpixelsArea(iftImage *labels)
{
    int numSuperpixels = 0;
    for (int i = 0; i < labels->ysize; ++i)
    {
        for (int j = 0; j < labels->xsize; ++j)
        {
            int label = iftImgVal(labels, j, i, 0);
            if (label > numSuperpixels)
                numSuperpixels = label;
        }
    }
    numSuperpixels++;

    int *superpixel_sizes = (int *)calloc(numSuperpixels, sizeof(int));

    // compute superpixels' area
    for (int i = 0; i < labels->n; ++i)
    {
        int spx = labels->val[i];
        if (spx > -1)
        {
            if (spx > numSuperpixels)
                iftError("Superpixel label is greater than the number of superpixels.", "mergeSpxBasedOnSize");
            superpixel_sizes[spx]++; // compute area
        }
    }
    return superpixel_sizes;
}

double computeRegularity(iftImage *labels)
{
    int superpixels = 0;
    int num_filtered_spx = 0;
    double std_dev = 0;

    for (int i = 0; i < labels->ysize; ++i)
    {
        for (int j = 0; j < labels->xsize; ++j)
        {
            int label = iftImgVal(labels, j, i, 0);
            if (label > superpixels) superpixels = label;
        }
    }
    superpixels++;

    int *area = computeSuperpixelsArea(labels);
    double mean_area = 0;

    for (int i = 0; i < superpixels; i++)
    {
        if (area[i] > 0) mean_area += (double)area[i];
        else num_filtered_spx++;
    }

    mean_area /= (superpixels - num_filtered_spx);

    for (int i = 0; i < superpixels; i++)
    {
        if (area[i] > 0) std_dev += ((mean_area-(double)area[i])*(mean_area-(double)area[i]));
    }

    std_dev /= (superpixels - num_filtered_spx);
    std_dev = sqrt(std_dev);
    free(area);
    return std_dev;
}

double computeRegularity_bkp(iftImage *labels)
{
    int superpixels = 0;
    int num_filtered_spx = 0;
    double std_dev = 0;

    for (int i = 0; i < labels->ysize; ++i)
    {
        for (int j = 0; j < labels->xsize; ++j)
        {
            int label = iftImgVal(labels, j, i, 0);
            if (label > superpixels) superpixels = label;
        }
    }
    superpixels++;

    int *area = computeSuperpixelsArea(labels);
    float sum_area_squared = 0;
    float sum_area = 0;

    for (int i = 0; i < labels->ysize; i++)
    {
        for (int j = 0; j < labels->xsize; j++)
        {
            int label = iftImgVal(labels, j, i, 0);
            if (label != -1) area[label] += 1;
        }
    }

    for (int i = 0; i < superpixels; i++)
    {
        if (area[i] > 0)
        {
            sum_area_squared += (float)(area[i] * area[i]);
            sum_area += (float)area[i];
        }
    }

    std_dev = sqrtf((sum_area_squared / superpixels) - ((sum_area / superpixels) * (sum_area / superpixels)));
    free(area);
    return std_dev;
}

//==========================================================

iftImage *ovlayBorders(iftImage *orig_img, iftImage *label_img, iftImage *gt_img,
                       float thick, iftColor color, iftColor gtcolor, iftColor overlap_color)
{
#if IFT_DEBUG //-----------------------------------------------------------//
    assert(orig_img != NULL);
    assert(label_img != NULL);
    iftVerifyImageDomains(orig_img, label_img, "ovlayBorders");
    assert(thick > 0);
#endif //------------------------------------------------------------------//
    int depth;
    iftImage *ovlay_img;
    iftAdjRel *A;

    A = iftCircular(thick);

    depth = iftImageDepth(orig_img);
    ovlay_img = iftCreateColorImage(orig_img->xsize, orig_img->ysize,
                                    orig_img->zsize, depth);

#if IFT_OMP //-------------------------------------------------------------//
#pragma omp parallel for
#endif //------------------------------------------------------------------//
    for (int p = 0; p < ovlay_img->n; ++p)
    {
        bool is_border_spx, is_border_gt;
        int i;
        iftVoxel p_vxl;

        is_border_spx = false, is_border_gt = false;
        p_vxl = iftGetVoxelCoord(ovlay_img, p);

        i = 0;
        while (is_border_spx == false && i < A->n)
        {
            iftVoxel adj_vxl = iftGetAdjacentVoxel(A, p_vxl, i);

            if (iftValidVoxel(ovlay_img, adj_vxl) == true)
            {
                int adj_idx = iftGetVoxelIndex(ovlay_img, adj_vxl);

                if (label_img->val[p] != label_img->val[adj_idx])
                    is_border_spx = true;
            }

            ++i;
        }

        if(gt_img != NULL){
            i = 0;
            while (is_border_gt == false && i < A->n)
            {
                iftVoxel adj_vxl = iftGetAdjacentVoxel(A, p_vxl, i);
                if (iftValidVoxel(ovlay_img, adj_vxl) == true)
                {
                    int adj_idx = iftGetVoxelIndex(ovlay_img, adj_vxl);

                    if (gt_img->val[p] != gt_img->val[adj_idx])
                        is_border_gt = true;
                }
                ++i;
            }
        }

        if (is_border_spx == true)
        {
            if(is_border_gt)
            {
                ovlay_img->val[p] = overlap_color.val[0];
                ovlay_img->Cb[p] = overlap_color.val[1];
                ovlay_img->Cr[p] = overlap_color.val[2];
            }else{
                ovlay_img->val[p] = color.val[0];
                ovlay_img->Cb[p] = color.val[1];
                ovlay_img->Cr[p] = color.val[2];
            }
        }
        else
        {
            if(is_border_gt){
                ovlay_img->val[p] = gtcolor.val[0];
                ovlay_img->Cb[p] = gtcolor.val[1];
                ovlay_img->Cr[p] = gtcolor.val[2];
            }else{
                ovlay_img->val[p] = orig_img->val[p];
                if (iftIsColorImage(orig_img) == true)
                {
                    ovlay_img->Cb[p] = orig_img->Cb[p];
                    ovlay_img->Cr[p] = orig_img->Cr[p];
                }
            }
        }
    }
    iftDestroyAdjRel(&A);

    if (depth != 8)
        iftConvertNewBitDepth(&ovlay_img, 8);

    return ovlay_img;
}



int check(int *v, int n)
{
    int i;
    for (i = 0; i < n; i++)
    {
        if (v[i] != -1)
        {
            return -1;
            break;
        }
        else
            return 1;
    }
}

void sortVector(int *v, int n)
{
    int i, k, j = 0, vp[n];
    int maxIndex = 0; // you need to add this variable in order to keep track of the maximum value in each iteration

    while (check(v, n) == -1)
    {
        for (i = 0; i < n; i++)
        {
            maxIndex = i; // you suppose that the maximum is the first element in each loop
            for (k = i + 1; k < n; k++)
            {
                if (v[k] < v[maxIndex])
                    maxIndex = k; // if there is another element greater you preserve its index in the variable
            }
            // after finishing the loop above you have the greatest variable in the array  which has the index stored in maxIndex
            vp[i] = v[maxIndex]; // put it in vp array
            v[maxIndex] = v[i];  // put it in treated elements zone
            v[i] = -1;           // make it -1
            j++;
        }
    }

    for (i = 0; i < n; i++)
        v[i] = vp[i];
}

iftImage *merge(const iftImage *ovlay_img1, const iftImage *ovlay_img2,
                float thick, int *distances)
{
#if IFT_DEBUG //-----------------------------------------------------------//
    assert(orig_img != NULL);
    assert(label_img != NULL);
    iftVerifyImageDomains(orig_img, label_img, "ovlayBorders");
    assert(thick > 0);
#endif //------------------------------------------------------------------//
    int depth, norm_val;
    iftImage *result_img;
    iftAdjRel *A;
    iftColor RGB, YCbCr;

    RGB.val[0] = RGB.val[1] = RGB.val[2] = 255;
    YCbCr = iftRGBtoYCbCr(RGB, 255);

    A = iftCircular(thick);

    depth = iftImageDepth(ovlay_img1);
    norm_val = iftMaxImageRange(depth);
    result_img = iftCreateColorImage(ovlay_img1->xsize, ovlay_img1->ysize,
                                     ovlay_img1->zsize, depth);

    // draw line: https://stackoverflow.com/questions/64561090/algorithm-for-drawing-diagonals-in-a-picture
    /*
      aspect = (float)c/r
      for (int i = 0; i < r; i++)
        matrix[i][(int)(aspect*i)] = color_of_diagonal
    */

    bool usingImg1 = true;

    // x -> rows -> ysize  ;  y -> cols -> xsize
    int x0 = distances[1] >= 0 && distances[1] < ovlay_img1->ysize ? distances[1] : ovlay_img1->ysize - 1; // default: last row
    int y0 = 0;                                                                                            // default: first col
    int x1 = 0;                                                                                            // default: first row
    int y1 = distances[0] >= 0 && distances[0] < ovlay_img1->xsize ? distances[0] : ovlay_img1->xsize - 1; // default: last col

    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = (dx > dy ? dx : -dy) / 2, e2;

    int pointsLine[ovlay_img1->n];
    for (int i = 0; i < ovlay_img1->n; i++)
        pointsLine[i] = 0;

    int numPoints = 0;
    for (int i = 0; i < ovlay_img1->n; i++)
    {
        int p = x0 * ovlay_img1->xsize + y0;

        pointsLine[p] = 1;
        numPoints++;

        if (x0 == x1 && y0 == y1)
            break;

        e2 = err;
        if (e2 > -dx)
        {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dy)
        {
            err += dx;
            y0 += sy;
        }
    }

    // sortVector(pointsLine, numPoints);

    // #if IFT_OMP //-------------------------------------------------------------//
    // #pragma omp parallel for
    // #endif //------------------------------------------------------------------//
    usingImg1 = true;
    for (int p = 0; p < result_img->n; ++p)
    {
        int x = p % ovlay_img1->xsize;

        if (x == 0)
            usingImg1 = true;

        if (pointsLine[p] == 1)
        {
            usingImg1 = false;
        }
        else
        {
            if (usingImg1)
            {
                result_img->val[p] = ovlay_img1->val[p];
                result_img->Cb[p] = ovlay_img1->Cb[p];
                result_img->Cr[p] = ovlay_img1->Cr[p];
            }
            else
            {
                result_img->val[p] = ovlay_img2->val[p];
                result_img->Cb[p] = ovlay_img2->Cb[p];
                result_img->Cr[p] = ovlay_img2->Cr[p];
            }
        }
    }

    for (int i = 0; i < result_img->n; i++)
    {
        if (pointsLine[i] != 1)
            continue;

        result_img->val[i] = YCbCr.val[0];
        result_img->Cb[i] = YCbCr.val[1];
        result_img->Cr[i] = YCbCr.val[2];

        int j;
        iftVoxel p_vxl;
        p_vxl = iftGetVoxelCoord(ovlay_img1, i);

        j = 0;
        while (j < A->n)
        {
            iftVoxel adj_vxl;
            adj_vxl = iftGetAdjacentVoxel(A, p_vxl, j);

            if (iftValidVoxel(ovlay_img1, adj_vxl) == true)
            {
                int adj_idx;

                adj_idx = iftGetVoxelIndex(ovlay_img1, adj_vxl);

                result_img->val[adj_idx] = YCbCr.val[0];
                result_img->Cb[adj_idx] = YCbCr.val[1];
                result_img->Cr[adj_idx] = YCbCr.val[2];
            }
            ++j;
        }
    }

    iftDestroyAdjRel(&A);

    if (depth != 8)
        iftConvertNewBitDepth(&result_img, 8);

    return result_img;
}

void qualitative(iftImage *orig,
                 iftImage *label_img,
                 iftImage *label_img2,
                 char *save_path,
                 Args args)
{
    float thick;
    iftImage *ovlay_img, *ovlay_img2, *result_img;

    iftColor RGB, YCbCr;
    RGB.val[0] = args.rgb[0];
    RGB.val[1] = args.rgb[1];
    RGB.val[2] = args.rgb[2];
    YCbCr = iftRGBtoYCbCr(RGB, 255);

    if (label_img2 != NULL)
    {
        ovlay_img = ovlayBorders(orig, label_img, NULL, args.thick, YCbCr, YCbCr, YCbCr);
        ovlay_img2 = ovlayBorders(orig, label_img2, NULL, args.thick, YCbCr, YCbCr, YCbCr);
        result_img = merge(ovlay_img, ovlay_img2, 4 * args.thick, args.distances);
        iftWriteImageByExt(result_img, save_path);
        iftDestroyImage(&ovlay_img);
        iftDestroyImage(&ovlay_img2);
        iftDestroyImage(&result_img);
    }
    else
    {
        ovlay_img = ovlayBorders(orig, label_img, NULL, args.thick, YCbCr, YCbCr, YCbCr);
        iftWriteImageByExt(ovlay_img, save_path);
        iftDestroyImage(&ovlay_img);
    }
}

//==========================================================

void drawtorect(cv::Mat &mat, cv::Rect target, int face, int thickness, cv::Scalar color, const std::string &str)
{
    Size rect = getTextSize(str, face, 1.0, thickness, 0);

    target.width -= 8;
    target.height -= 8;
    target.x += 4;
    target.y += 4;

    if (target.height < 10 || ((float)target.width / (float)target.height < 1.8 && target.width < 36))
        return;
    if (target.width > 60)
    {
        target.x += (target.width - 60) / 2;
        target.width = 60;
    }
    if (target.height > 13)
    {
        target.y += (target.height - 13) / 2;
        target.height = 13;
    }

    double scalex = (double)target.width / (double)rect.width;
    double scaley = (double)target.height / (double)rect.height;
    double scale = min(scalex, scaley);
    int marginx = scale == scalex ? 0 : (int)((double)target.width * (scalex - scale) / scalex * 0.5);
    int marginy = scale == scaley ? 0 : (int)((double)target.height * (scaley - scale) / scaley * 0.5);

    putText(mat, str, Point(target.x + marginx, target.y + target.height - marginy), face, scale, color, thickness, 8, false);
}

Rect findMinRect(const Mat1b &src)
{
    Mat1f W(src.rows, src.cols, float(0));
    Mat1f H(src.rows, src.cols, float(0));

    Rect maxRect(0, 0, 0, 0);
    float maxArea = 0.f;

    for (int r = 0; r < src.rows; ++r)
    {
        for (int c = 0; c < src.cols; ++c)
        {
            if (src(r, c) == 0)
            {
                H(r, c) = 1.f + ((r > 0) ? H(r - 1, c) : 0);
                W(r, c) = 1.f + ((c > 0) ? W(r, c - 1) : 0);
            }

            float minw = W(r, c);
            for (int h = 0; h < H(r, c); ++h)
            {
                minw = min(minw, W(r - h, c));
                float area = (h + 1) * minw;
                if (area > maxArea)
                {
                    maxArea = area;
                    maxRect = Rect(Point(c - minw + 1, r - h), Point(c + 1, r + 1));
                }
            }
        }
    }

    return maxRect;
}

void createImageMetric(iftImage *L, double *colorVariance, int num_rows, int num_cols, const char *filename, bool showScores)
{
    /*
    L : label map
    colorVariance : segmentaton error
    */

    NodeAdj *AdjRel;
    Mat image;
    int K;

    K = 0;
    for (int p = 0; p < num_rows * num_cols; p++)
        K = MAX(K, L->val[p]);
    K++;

    int superpixel_size[K];
    TextsConfig textsConfig[K];
    vector<vector<Point>> contours(K);
    int count_contours[K];

#ifdef DEBUG
    printf("createImageMetric: Alloc structures\n");
#endif

    AdjRel = create8NeighAdj();
    image = Mat::zeros(num_rows, num_cols, CV_8UC3);

    int max_Label = 0;
    for (int s = 0; s < K; s++)
    {
        superpixel_size[s] = 0;
        count_contours[s] = 0;
    }
    for (int p = 0; p < num_rows * num_cols; p++)
    {
        superpixel_size[L->val[p]]++;
        max_Label = MAX(max_Label, L->val[p]);
    }

    for (int s = 0; s < K; s++)
        contours[s] = vector<Point>(superpixel_size[s]);

#ifdef DEBUG
    printf("Iterate over the image pixels.. \n");
#endif
    for (int p = 0; p < num_rows * num_cols; p++)
    {
        Vec3b color, border;
        NodeCoords coords;
        int label = L->val[p];

        color[0] = color[1] = color[2] = (int)(255 * MIN(1, colorVariance[label])); // get superpixel color according to its error
        border[0] = border[1] = border[2] = (color[0] < 128) ? 255 : 0;

        bool isBorder = false;
        coords = getNodeCoordsImage(num_cols, p);

        for (int j = 0; j < AdjRel->size; j++)
        {
            NodeCoords adj_coords;
            adj_coords = getAdjacentNodeCoords(AdjRel, coords, j);

            if (areValidNodeCoordsImage(num_rows, num_cols, adj_coords))
            {
                int adj_index;
                adj_index = getNodeIndexImage(num_cols, adj_coords);
                if (label != L->val[adj_index])
                {
                    isBorder = true;
                    if (image.at<Vec3b>(Point(adj_coords.x, adj_coords.y))[0] == 0 && image.at<Vec3b>(Point(adj_coords.x, adj_coords.y))[0] == 255)
                        image.at<Vec3b>(Point(coords.x, coords.y)) = border;
                    else
                        image.at<Vec3b>(Point(coords.x, coords.y)) = image.at<Vec3b>(Point(adj_coords.x, adj_coords.y));
                    break;
                }
            }
        }
        if (!isBorder)
            image.at<Vec3b>(Point(coords.x, coords.y)) = color;

        contours[label][count_contours[label]] = Point(coords.x, coords.y);
        count_contours[label]++;

        gcvt(colorVariance[label], 2, textsConfig[label].text);
        textsConfig[label].textColor[0] = color[0] < 128 ? 255 : 0;
        textsConfig[label].textColor[1] = textsConfig[label].textColor[2] = textsConfig[label].textColor[0];
    }

#ifdef DEBUG
    printf("Apply colormap.. \n");
#endif
    // Apply the colormap
    applyColorMap(image, image, COLORMAP_OCEAN);

    if (showScores)
    {
        vector<vector<Point>> contours_poly(contours.size());
        vector<Rect> boundRect(contours.size());

#ifdef DEBUG
        printf("Draw scores.. \n");
#endif

        // draw scaled text according to region size: https://stackoverflow.com/questions/50353884/calculate-text-size
        // multidimentional vectors in c++: https://www.geeksforgeeks.org/2d-vector-in-cpp-with-user-defined-size/
        // fin min enclosed rectangle: https://stackoverflow.com/questions/34896431/creating-rectangle-within-a-blob-using-opencv

        for (size_t i = 0; i < contours.size(); ++i)
        {
            if (contours[i].size() > 0)
            {
                // Create a mask for each single blob
                Mat1b maskSingleContour(num_rows, num_cols, uchar(0));

                for (size_t j = 0; j < contours[i].size(); j++)
                {
                    maskSingleContour.at<uchar>(contours[i][j]) = 255;
                }

                // Find minimum rect for each blob
                Rect box = findMinRect(~maskSingleContour);

                // Draw rect
                drawtorect(image, box, cv::FONT_HERSHEY_PLAIN, 1, textsConfig[i].textColor, textsConfig[i].text);
            }
        }
    }

#ifdef DEBUG
    printf("Write final image.. \n");
#endif

    imwrite(filename, image);

#ifdef DEBUG
    printf("Free structure.. \n");
#endif

    freeNodeAdj(&AdjRel);
}

//==========================================================

iftImage *readRGBImage(char *filepath)
{
    iftImage *image, *rgb_image;
    iftColor YCbCr, RGB;

    image = iftReadImageByExt(filepath);
    rgb_image = iftCreateColorImage(image->xsize, image->ysize, image->zsize, 255);

    for (int p = 0; p < image->n; p++)
    {

        if (iftIsColorImage(image))
        {
            YCbCr.val[0] = image->val[p];
            YCbCr.val[1] = image->Cb[p];
            YCbCr.val[2] = image->Cr[p];

            RGB = iftYCbCrtoRGB(YCbCr, 255);

            rgb_image->val[p] = RGB.val[0];
            rgb_image->Cb[p] = RGB.val[1];
            rgb_image->Cr[p] = RGB.val[2];
        }
        else
        {
            rgb_image->val[0] = rgb_image->val[1] = rgb_image->val[2] = image->val[p];
        }
    }
    iftDestroyImage(&image);
    return rgb_image;
}

iftImage *readCSVImage(char *filepath, int xsize, int ysize, int zsize)
{
    FILE *fp;
    iftImage *img;
    char ch;

    if (xsize != 0 && ysize != 0)
        img = iftCreateImage(xsize, ysize, zsize);
    else
    {
        fp = fopen(filepath, "r");
        if (fp == NULL)
            printError("readCSVImage", "Could not open the file <%s>", filepath);

        xsize = 0;
        ysize = 0;
        while (!feof(fp))
        {
            ch = fgetc(fp);
            if (ch == '\n')
                ysize++;
            else if (ch != ',')
                xsize++;
        }
        fclose(fp);
        img = iftCreateImage(xsize, ysize, zsize);
    }

    fp = fopen(filepath, "r");
    if (fp == NULL)
        printError("readCSVImage", "Could not open the file <%s>", filepath);

    int index = 0, val = 0;
    int num_rows = 0, num_cols = 0;
    while (!feof(fp))
    {
        ch = fgetc(fp);
        if (ch == ',' || ch != '\n')
        {
            if (ch == ',')
                num_cols++;
            else
            {
                num_rows++;
                if (num_cols != ysize)
                    printError("readCSVImage", "Number of columns do not match: %d %d", num_cols, ysize);
            }
            img->val[index] = val;
            val = 0;
            index++;
        }
        else
            val = val * 10 + (ch - '0');
    }
    fclose(fp);

    if (num_rows != xsize)
        printError("readCSVImage", "Number of rows do not match: %d %d", num_rows, xsize);

    return img;
}

// set -1 in the superpixels (at pixel level) that have the same color as the ignoreColor in the gt image.
// return the number of superpixels after filtering
int removeSuperpixelsByGTColor(iftImage *labels, iftImage *gt, int removeColor)
{
    for (int i = 0; i < gt->n; ++i)
    {
        if (gt->val[i] == removeColor)
        {
            labels->val[i] = -1;
        }
    }

    return getNumSuperpixels(labels);
}

// the function's name is not coherent with what it does. It counts the number of superpixels smaller than a threshold.
// The function numSpxSmallerThan does the same thing.
int removeSuperpixelsBySize(iftImage *labels, iftImage *gt, int sizeThreshold)
{
    int *superpixel_sizes;
    int numSmallSuperpixels = 0;
    int superpixels = getNumSuperpixels(labels);

    superpixel_sizes = computeSuperpixelsArea(labels);

    for (int i = 0; i < superpixels; i++)
    {
        if (superpixel_sizes[i] < sizeThreshold && superpixel_sizes[i] > 0)
        {
            numSmallSuperpixels++;
        }
    }

    return numSmallSuperpixels;
}

/* Cut superpixels based on a color in a mask image */
int recreateLabels(iftImage *labels, iftImage *gt, iftColor removeColor)
{
    // get max label
    int num_spx = relabelSuperpixels(labels, 8);
    
    // relabel "labels" based on "removeColor" in "gt"
    for (int i = 0; i < labels->n; ++i)
    {
        if (gt->val[i] == removeColor.val[0] && gt->Cb[i] == removeColor.val[1] && gt->Cr[i] == removeColor.val[2])
            labels->val[i] = -1;
    }
    
    // relabel connected components after set some pixels in labels to -1
    num_spx = relabelSuperpixels(labels, 8);
    
    // relabel "labels" based on "removeColor" in "gt"
    for (int i = 0; i < labels->n; ++i)
    {
        if (gt->val[i] == removeColor.val[0] && gt->Cb[i] == removeColor.val[1] && gt->Cr[i] == removeColor.val[2])
            labels->val[i] = num_spx;
    }

    // relabel connected componets again, since the -1 areas may be unconnected
    return relabelSuperpixels(labels, 8);
}

// replace to newColor pixels in orig_img that have oldColor in gt_image
void replaceColor(iftImage *orig_img, iftImage *gt_img, iftColor oldColor, iftColor newColor)
{
    for (int i = 0; i < orig_img->n; i++)
    {
        if (gt_img->val[i] == oldColor.val[0] && gt_img->Cb[i] == oldColor.val[1] && gt_img->Cr[i] == oldColor.val[2])
        {
            orig_img->val[i] = newColor.val[0];
            orig_img->Cb[i] = newColor.val[1];
            orig_img->Cr[i] = newColor.val[2];
        }
    }
}

void replaceColorDir(char *img_path, char *gt_path, char *save_path)
{

    if (!iftDirExists(img_path))
        iftError("Image directory not found", __func__);
    if (!iftDirExists(gt_path))
        iftError("Ground truth directory not found", __func__);
    iftColor oldColor, newColor;

    oldColor.val[0] = 16;
    oldColor.val[1] = 128;
    oldColor.val[2] = 128;

    newColor.val[0] = 16;
    newColor.val[1] = 128;
    newColor.val[2] = 128;

    iftFileSet *img_files = iftLoadFileSetFromDir(img_path, 1);
    for (int i = 0; i < img_files->n; i++)
    {
        char *basename, ext[10], filename[200];

        // get file extension and image name
        sprintf(ext, "%s", iftFileExt(img_files->files[i]->path));
        basename = iftFilename(img_files->files[i]->path, ext);
        sprintf(filename, "%s/%s%s", gt_path, basename, ext);

        // read original image and grund truth
        iftImage *orig_img = iftReadImageByExt(img_files->files[i]->path);
        iftImage *gt_img = iftReadImageByExt(filename);

        replaceColor(orig_img, gt_img, oldColor, newColor);

        // write new image
        sprintf(filename, "%s/%s%s", save_path, basename, ext);
        iftWriteImageByExt(orig_img, filename);

        iftFree(basename);
    }
    iftDestroyFileSet(&img_files);
}

// Return the number of superpixels smaller than a threshold
int numSpxSmallerThan(iftImage *labels, int threshold)
{
    int num_spx = getNumSuperpixels(labels);
    int *spx_areas = computeSuperpixelsArea(labels);
    int cont = 0;

    for (int i = 0; i < num_spx; i++)
    {
        if (spx_areas[i] > 0 && spx_areas[i] < threshold)
            cont++;
    }
    free(spx_areas);
    return cont;
}

/* Cut superpixels based on a color in a mask image */
void recreateLabelsDir(char *labels_path, char *gt_path, char *save_path)
{
    if (!iftDirExists(labels_path))
        iftError("Labels directory not found", __func__);
    if (!iftDirExists(gt_path))
        iftError("Ground truth directory not found", __func__);
    if (!iftDirExists(save_path))
        iftMakeDir(save_path);

    //printf("labels_path:%s \n", labels_path);
    //printf("gt_path:%s \n", gt_path);
    //printf("save_path:%s \n", save_path);

    iftColor color;
    color.val[0] = 16;
    color.val[1] = 128;
    color.val[2] = 128;

    iftFileSet *labels_files = iftLoadFileSetFromDir(labels_path, 1);
    for (int i = 0; i < labels_files->n; i++)
    {
        char *basename, ext[10], filename[200];

        // get file extension and image name
        sprintf(ext, "%s", iftFileExt(labels_files->files[i]->path));
        basename = iftFilename(labels_files->files[i]->path, ext);
        sprintf(filename, "%s/%s.png", gt_path, basename);

        // read original image and grund truth
        //printf("read labels: %s \n", labels_files->files[i]->path);
        iftImage *labels_img = iftReadImageByExt(labels_files->files[i]->path);
        //printf("read gt: %s \n", filename);
        iftImage *gt_img = iftReadImageByExt(filename);

        int numSpx = recreateLabels(labels_img, gt_img, color);

        // write new image
        sprintf(filename, "%s/%s%s", save_path, basename, ext);
        //printf("write new labels: %s \n", filename);
        iftWriteImageByExt(labels_img, filename);

        iftDestroyImage(&labels_img);
        iftDestroyImage(&gt_img);
        iftFree(basename);
    }
    iftDestroyFileSet(&labels_files);
}

int mergeSpxBasedOnSize(iftImage *labels, iftImage *orig_img, int min_size,
                        iftImage *gt_image, iftColor color, char *tmp_save_path)
{
    int numSuperpixels = 0;

    // avoid (cut) superpixels with pixels crossing regions with "color" in gt
    /*if (gt_image != NULL)
        numSuperpixels = recreateLabels(labels, gt_image, color);
    else*/
        numSuperpixels = relabelSuperpixels(labels, 8);

    double *superpixel_sizes = (double *)calloc(numSuperpixels, sizeof(double));
    int *parent_spx = (int *)calloc(numSuperpixels, sizeof(int));
    int **adjacencyMatrixSpx = (int **)calloc(numSuperpixels, sizeof(int *));
    int sum_color[numSuperpixels][3];
    NodeAdj *adj_rel = create8NeighAdj();
    int num_merged_spx = 0;

    iftDHeap *heap = iftCreateDHeap(numSuperpixels, superpixel_sizes);
    heap->removal_policy = MINVAL_POLICY;

    for (int i = 0; i < numSuperpixels; i++)
    {
        parent_spx[i] = i; // initialize parent. It'll change if the superpixel is merged
        sum_color[i][0] = 0;
        sum_color[i][1] = 0;
        sum_color[i][2] = 0;
        adjacencyMatrixSpx[i] = (int *)calloc(numSuperpixels, sizeof(int));
    }

    // compute superpixels' area, sum color, and adjacency matrix
    for (int i = 0; i < labels->n; ++i)
    {
        int spx = labels->val[i];

        // ignore pixels in gt with color "color"
        if (gt_image == NULL || gt_image->val[i] != color.val[0] || gt_image->Cb[i] != color.val[1] || gt_image->Cr[i] != color.val[2])
        {
            if (spx > numSuperpixels){
                iftError("Superpixel label is greater than the number of superpixels.", "mergeSpxBasedOnSize");
            }if (spx < 0){
                printf("spx: %d color: %d %d %d \n", spx, gt_image->val[i], gt_image->Cb[i], gt_image->Cr[i]);
                iftError("Superpixel label is negative.", "mergeSpxBasedOnSize");
            }
            superpixel_sizes[spx]++; // compute area

            // compute sum of colors
            sum_color[spx][0] += orig_img->val[i];
            sum_color[spx][1] += orig_img->Cb[i];
            sum_color[spx][2] += orig_img->Cr[i];

            NodeCoords vertexCoords;
            vertexCoords = getNodeCoordsImage(labels->xsize, i);

            // there is any different spx label in neighbor pixels?
            for (int j = 0; j < adj_rel->size; j++)
            {
                NodeCoords adjCoords;
                adjCoords = getAdjacentNodeCoords(adj_rel, vertexCoords, j);

                if (areValidNodeCoordsImage(labels->ysize, labels->xsize, adjCoords))
                {
                    int adjVertex = getNodeIndexImage(labels->xsize, adjCoords);
                    // if the neighbor pixel has a different label, set the adjacency matrix
                    if (labels->val[adjVertex] != spx && labels->val[adjVertex] > -1 &&
                        (gt_image == NULL || gt_image->val[adjVertex] != color.val[0] || gt_image->Cb[adjVertex] != color.val[1] || gt_image->Cr[adjVertex] != color.val[2]))
                    {
                        adjacencyMatrixSpx[spx][labels->val[adjVertex]] = 1;
                        adjacencyMatrixSpx[labels->val[adjVertex]][spx] = 1;
                    }
                }
            }
        }
    }

    // insert small superpixels in heap
    for (int i = 0; i < numSuperpixels; ++i)
    {
        if (superpixel_sizes[i] > 0 && superpixel_sizes[i] < min_size)
            iftInsertDHeap(heap, i);
    }

    // set which superpixels should merge
    while (!iftEmptyDHeap(heap))
    {
        int i = iftRemoveDHeap(heap);

        // compress path
        int old_parent = i;
        while (old_parent != parent_spx[old_parent])
        {
            old_parent = parent_spx[old_parent];
        }
        int tmp = i;
        while (tmp != parent_spx[tmp])
        {
            int p = parent_spx[tmp];
            parent_spx[tmp] = old_parent;
            tmp = p;
        }

        // if a auperpixel is too small, merge it
        if ((int)superpixel_sizes[old_parent] < min_size)
        {
            // compute mean color
            float spxColor[3];
            spxColor[0] = sum_color[i][0] / superpixel_sizes[old_parent];
            spxColor[1] = sum_color[i][1] / superpixel_sizes[old_parent];
            spxColor[2] = sum_color[i][1] / superpixel_sizes[old_parent];

            // find the neighbor superpixel with the most similar color
            float minDiff = IFT_INFINITY_FLT;
            int new_parent = old_parent;
            for (int j = 0; j < numSuperpixels; j++)
            {
                // compress path
                int parent_adj = j;
                while (parent_adj != parent_spx[parent_adj])
                {
                    parent_adj = parent_spx[parent_adj];
                }
                int tmp = j;
                while (tmp != parent_spx[tmp])
                {
                    tmp = parent_spx[tmp];
                    parent_spx[tmp] = parent_adj;
                }

                // evaluate if the spx is neighbor and offers the minimum difference in color
                if (old_parent != parent_adj && adjacencyMatrixSpx[i][j] == 1)
                {
                    float adjColor[3];
                    adjColor[0] = sum_color[parent_adj][0] / superpixel_sizes[parent_adj];
                    adjColor[1] = sum_color[parent_adj][1] / superpixel_sizes[parent_adj];
                    adjColor[2] = sum_color[parent_adj][1] / superpixel_sizes[parent_adj];

                    float diff = sqrtf((spxColor[0] - adjColor[0]) * (spxColor[0] - adjColor[0]) + (spxColor[1] - adjColor[1]) * (spxColor[1] - adjColor[1]) + (spxColor[2] - adjColor[2]) * (spxColor[2] - adjColor[2]));
                    if (diff < minDiff)
                    {
                        minDiff = diff;
                        new_parent = parent_adj;
                    }
                }
            }

            parent_spx[old_parent] = new_parent; // update spx parent

            if (old_parent != new_parent)
            {
                // compress path
                tmp = i;
                while (tmp != parent_spx[tmp])
                {
                    tmp = parent_spx[tmp];
                    parent_spx[tmp] = new_parent;
                }

                // update superpixel sizes and sum of colors
                superpixel_sizes[new_parent] += superpixel_sizes[old_parent];
                sum_color[new_parent][0] += sum_color[old_parent][0];
                sum_color[new_parent][1] += sum_color[old_parent][1];
                sum_color[new_parent][2] += sum_color[old_parent][2];
                num_merged_spx++;

                // update adjacency matrix
                for (int j = 0; j < numSuperpixels; j++)
                {
                    if (j != new_parent && adjacencyMatrixSpx[old_parent][j] == 1)
                    {
                        adjacencyMatrixSpx[new_parent][j] = 1;
                        adjacencyMatrixSpx[j][new_parent] = 1;
                    }
                }

                // is after mearging the size is still small, insert it in the heap
                if (superpixel_sizes[new_parent] < min_size)
                {
                    if (heap->color[new_parent] == IFT_GRAY)
                        iftRemoveDHeapElem(heap, new_parent);
                    if (heap->color[new_parent] == IFT_BLACK)
                        heap->color[new_parent] = IFT_WHITE;
                    iftInsertDHeap(heap, new_parent);
                }
            }
        }
    }

    // compress paths
    for (int i = 0; i < numSuperpixels; ++i)
    {
        // compress
        int parent = i;
        while (parent != parent_spx[parent])
        {
            parent = parent_spx[parent];
        }
        int tmp = i;
        while (tmp != parent_spx[tmp])
        {
            int p = parent_spx[tmp];
            parent_spx[tmp] = parent;
            tmp = p;
        }
    }

    // relabel superpixels
    for (int i = 0; i < labels->n; ++i)
        labels->val[i] = parent_spx[labels->val[i]];

    // save gt image with remaining small superpixels in blue
    if (tmp_save_path != NULL)
    {
        iftImage *tmp = iftCopyImage(gt_image);
        for (int i = 0; i < labels->n; ++i)
        {
            if (superpixel_sizes[labels->val[i]] < min_size && superpixel_sizes[labels->val[i]] > 0)
            {
                tmp->val[i] = 41;
                tmp->Cb[i] = 240;
                tmp->Cr[i] = 110;
            }
        }
        iftWriteImageByExt(tmp, tmp_save_path);
        iftDestroyImage(&tmp);
    }

    free(parent_spx);
    free(superpixel_sizes);
    for (int i = 0; i < numSuperpixels; ++i)
        free(adjacencyMatrixSpx[i]);
    free(adjacencyMatrixSpx);
    iftDestroyDHeap(&heap);
    freeNodeAdj(&adj_rel);

    return relabelSuperpixels(labels, 8);
}

/* Remove tiny superpixels from "labels", merging it with the most similar neighbor.
    Superpixels with any pixel with color in gt are ignored in this merging step */
void mergeSpxBasedOnSizeDir(char *orig_path, char *labels_path, char *gt_path, char *save_path)
{
    if (!iftDirExists(labels_path))
        iftError("Labels directory not found", __func__);
    if (!iftDirExists(gt_path))
        iftError("Ground truth directory not found", __func__);
    if (!iftDirExists(orig_path))
        iftError("Images directory not found", __func__);
    if (!iftDirExists(save_path))
        iftMakeDir(save_path);
    iftColor color;
    int min_size = 60;
    char ext_labels[10], ext_gt[10];

    color.val[0] = 16;
    color.val[1] = 128;
    color.val[2] = 128;

    iftFileSet *labels_files = iftLoadFileSetFromDir(labels_path, 1);
    sprintf(ext_labels, "%s", iftFileExt(labels_files->files[0]->path));
    iftDestroyFileSet(&labels_files);

    iftFileSet *gt_files = iftLoadFileSetFromDir(gt_path, 1);
    sprintf(ext_gt, "%s", iftFileExt(gt_files->files[0]->path));
    iftDestroyFileSet(&gt_files);

    iftFileSet *orig_files = iftLoadFileSetFromDir(orig_path, 1);
    for (int i = 0; i < orig_files->n; i++)
    {
        char *basename, ext[10], filename[200];

        // get file extension and image name
        sprintf(ext, "%s", iftFileExt(orig_files->files[i]->path));
        basename = iftFilename(orig_files->files[i]->path, ext);
        iftImage *orig_img = iftReadImageByExt(orig_files->files[i]->path);

        // read original image and grund truth
        sprintf(filename, "%s/%s%s", labels_path, basename, ext_labels);
        iftImage *labels_img = iftReadImageByExt(filename);

        sprintf(filename, "%s/%s%s", gt_path, basename, ext_gt);
        iftImage *gt_img = iftReadImageByExt(filename);

        // sprintf(filename, "%s/%s_small%s", save_path, basename, ext_gt);
        int num_spx = mergeSpxBasedOnSize(labels_img, orig_img, min_size, gt_img, color, NULL);

        // write new image
        sprintf(filename, "%s/%s%s", save_path, basename, ext_labels);
        iftWriteImageByExt(labels_img, filename);

        iftDestroyImage(&labels_img);
        iftDestroyImage(&gt_img);
        iftDestroyImage(&orig_img);
        iftFree(basename);
    }
    iftDestroyFileSet(&orig_files);
}

int filterDir(const struct dirent *name)
{
    int pos = 0;
    while (name->d_name[pos] != '.')
    {
        if (name->d_name[pos] == '\0')
            return 0;
        pos++;
    }
    pos++;
    if (name->d_name[pos] == '\0')
        return 0;

    int extSize = 0;
    while (name->d_name[pos + extSize] != '\0')
    {
        extSize++;
    }

    char ext[extSize];
    int pos2 = 0;
    while (pos2 < extSize)
    {
        ext[pos2] = name->d_name[pos + pos2];
        pos2++;
    }

    if ((extSize == 3 && ((ext[0] == 'p' && ext[1] == 'n' && ext[2] == 'g') ||
                          (ext[0] == 'j' && ext[1] == 'p' && ext[2] == 'g') ||
                          (ext[0] == 'p' && ext[1] == 'p' && ext[2] == 'm') ||
                          (ext[0] == 'p' && ext[1] == 'g' && ext[2] == 'm') ||
                          (ext[0] == 'b' && ext[1] == 'm' && ext[2] == 'p') ||
                          (ext[0] == 'c' && ext[1] == 's' && ext[2] == 'v'))) ||
        (extSize == 4 && ext[0] == 't' && ext[1] == 'i' && ext[2] == 'f' && ext[2] == 'f'))
        return 1;

    return 0;
}

void readFileInDir(char *file_name, char *dir, const char *ext, char *output)
{
    struct stat stats; // determine mode : file or path

    if (stat(dir, &stats) == -1)
        printError("readFileInDir", "directory %s not found.", dir);

    if ((stats.st_mode & S_IFMT) == S_IFDIR)
        sprintf(output, "%s/%s.%s", dir, file_name, ext);
    else
        printError("readFileInDir", "%s must be a directory.", dir);
}

/*!
 * \brief       Call the respective superpixel evaluation measure
 * \param       image_name      Image name with extension.
 *                              It can be the original image,
 *                              the ground truth, or NULL.
 * \param       args            Command line arguments
 * \param       numSuperpixels  (output) The number of labels on
 *                              the superpixel segmentation.
 * \result      The evaluation score or -1 for non-quantitative evaluation.
 * \date        Last update: 2019/08/28
 */
double eval(char *image_name, Args args, int *numSuperpixels)
{
    if (args.metric == 1)
    {
#ifdef DEBUG
        printf("SIRS \n"); // Added GT - Changed metric function - Appears to work
#endif
        iftImage *labels, *image, *gt = NULL;
        double *explainedVariation = NULL, score = 0;
        int maxLabel;
        char fileName[255], labels_path[255], img_path[255], gt_path[255];
        char *reconstruction_path;

        getImageName(image_name, fileName);
        readFileInDir(fileName, args.label_path, args.label_ext, labels_path);
        sprintf(img_path, "%s/%s", args.img_path, image_name);
        
        if (args.imgRecon != NULL)
        {
            reconstruction_path = (char *)malloc(255 * sizeof(char));
            readFileInDir(fileName, args.imgRecon, "png", reconstruction_path);
        }
        else
            reconstruction_path = NULL;

        labels = iftReadImageByExt(labels_path);
        image = readRGBImage(img_path);

        if (image->xsize != labels->xsize || image->ysize != labels->ysize || image->zsize != labels->zsize)
            printError("eval", "Image and labels must have the same size");

        if (args.removeColor != -1){
            sprintf(gt_path, "%s/%s", args.gt_path, image_name); // Used for mask
            gt = iftReadImageByExt(gt_path); // Used for mask
            removeSuperpixelsByGTColor(labels, gt, args.removeColor); // Used for mask
        }

        /*if (args.removeSize != -1)
        {
            smallSuperpixels = removeSuperpixelsBySize(labels, gt, args.removeSize); // Used for mask
        }*/

        (*numSuperpixels) = relabelSuperpixels(labels, 8);
        explainedVariation = SIRS(labels, image, 
                            args.alpha, args.buckets, reconstruction_path, args.gauss_variance, 
                            &score);
        if(gt != NULL) iftDestroyImage(&gt);
        iftDestroyImage(&image);
        if (args.imgRecon != NULL) free(reconstruction_path);

        if (args.imgScoresPath != NULL)
        {
            char *imgScores_path = (char *)malloc(255 * sizeof(char));
            readFileInDir(fileName, args.imgScoresPath, "png", imgScores_path);
            createImageMetric(labels, explainedVariation, labels->ysize, labels->xsize, imgScores_path, args.drawScores);
            free(imgScores_path);
        }
        free(explainedVariation);
        iftDestroyImage(&labels);

        return score;
    }

    if (args.metric == 2)
    {
#ifdef DEBUG
        printf("EV \n"); // Added GT - Changed function metric - Appears to work
#endif
        iftImage *labels, *image, *gt = NULL;
        double *explainedVariation = NULL, score = 0;
        char fileName[255], labels_path[255], img_path[255], gt_path[255];
        char *reconstruction_path;

        getImageName(image_name, fileName);
        readFileInDir(fileName, args.label_path, args.label_ext, labels_path);
        sprintf(img_path, "%s/%s", args.img_path, image_name);
        
        if (args.imgRecon != NULL)
        {
            reconstruction_path = (char *)malloc(255 * sizeof(char));
            readFileInDir(fileName, args.imgRecon, "png", reconstruction_path);
        }
        else
            reconstruction_path = NULL;

        labels = iftReadImageByExt(labels_path);
        image = readRGBImage(img_path);

        if (image->xsize != labels->xsize || image->ysize != labels->ysize || image->zsize != labels->zsize)
            printError("eval", "Image and labels must have the same size");

        if (args.removeColor != -1){
            sprintf(gt_path, "%s/%s", args.gt_path, image_name); // Used for mask
            gt = iftReadImageByExt(gt_path); // Used for mask
            removeSuperpixelsByGTColor(labels, gt, args.removeColor); // Used for mask
        }

        /*if (args.removeSize != -1)
        {
            removeSuperpixelsBySize(labels, gt, args.removeSize); // Used for mask
        }*/

        if(gt != NULL) iftDestroyImage(&gt);

        (*numSuperpixels) = relabelSuperpixels(labels, 8);
        explainedVariation = computeExplainedVariation(labels, image, reconstruction_path, &score);

        iftDestroyImage(&image);
        if (args.imgRecon != NULL)
            free(reconstruction_path);

        if (args.imgScoresPath != NULL)
        {
            char *imgScores_path = (char *)malloc(255 * sizeof(char));
            readFileInDir(fileName, args.imgScoresPath, "png", imgScores_path);
            createImageMetric(labels, explainedVariation, labels->ysize, labels->xsize, imgScores_path, args.drawScores);
            free(imgScores_path);
        }
        free(explainedVariation);
        iftDestroyImage(&labels);

        return score;
    }

    if (args.metric == 3)
    {
#ifdef DEBUG
        printf("BR \n"); // Changed function metric - Appears to work
#endif
        iftImage *labels, *gt = NULL;
        double score = 0;
        char fileName[255], labels_path[255], img_path[255];

        getImageName(image_name, fileName);
        readFileInDir(fileName, args.label_path, args.label_ext, labels_path);
        sprintf(img_path, "%s/%s", args.img_path, image_name);

        labels = iftReadImageByExt(labels_path);
        gt = iftReadImageByExt(img_path);

        if (gt->xsize != labels->xsize || gt->ysize != labels->ysize || gt->zsize != labels->zsize)
            printError("eval", "gt image and labels must have the same size");

        (*numSuperpixels) = getNumSuperpixels(labels);

        if (args.removeColor != -1)
            removeSuperpixelsByGTColor(labels, gt, args.removeColor); // Used for mask
        
        
        /*if (args.removeSize != -1)
        {
            removeSuperpixelsBySize(labels, gt, args.removeSize); // Used for mask
        }*/

        (*numSuperpixels) = relabelSuperpixels(labels, 8);
        score = computeBoundaryRecall(labels, gt, 0.0025);
        iftDestroyImage(&gt);
        iftDestroyImage(&labels);
        return score;
    }

    if (args.metric == 4)
    {
#ifdef DEBUG
        printf("UE \n"); // Changed function metric - Appears to be working - Verify if N(area of GT) should also be changed
#endif
        iftImage *labels, *gt = NULL;
        double score = 0;
        char fileName[255], labels_path[255], img_path[255];

        getImageName(image_name, fileName);
        readFileInDir(fileName, args.label_path, args.label_ext, labels_path);
        sprintf(img_path, "%s/%s", args.img_path, image_name);

        labels = iftReadImageByExt(labels_path);
        gt = iftReadImageByExt(img_path);

        if (gt->xsize != labels->xsize || gt->ysize != labels->ysize || gt->zsize != labels->zsize)
            printError("eval", "gt image and labels must have the same size");

        if (args.removeColor != -1)
            removeSuperpixelsByGTColor(labels, gt, args.removeColor); // Used for mask
        
        
        /*if (args.removeSize != -1)
        {
            smallSuperpixels = removeSuperpixelsBySize(labels, gt, args.removeSize); // Used for mask
            (*numSuperpixels) -= smallSuperpixels;
        }*/

        (*numSuperpixels) = relabelSuperpixels(labels, 8);
        score = computeUndersegmentationError(labels, gt);
        iftDestroyImage(&gt);
        iftDestroyImage(&labels);
        return score;
    }

    if (args.metric == 5)
    {
#ifdef DEBUG
        printf("CO \n"); // Added GT - Changed function metric - Works, but need to check num of rows and cols of labels
#endif
        iftImage *labels, *gt = NULL;
        double score = 0;
        char fileName[255], labels_path[255], gt_path[255];

        getImageName(image_name, fileName);
        readFileInDir(fileName, args.label_path, args.label_ext, labels_path);
        labels = iftReadImageByExt(labels_path);
        
        if (args.removeColor != -1){
            sprintf(gt_path, "%s/%s", args.gt_path, image_name); // Used for mask
            gt = iftReadImageByExt(gt_path); // Used for mask
            removeSuperpixelsByGTColor(labels, gt, args.removeColor); // Used for mask
        }
        
        /*if (args.removeSize != -1)
        {
            removeSuperpixelsBySize(labels, gt, args.removeSize); // Used for mask
        }*/

        if(gt != NULL) iftDestroyImage(&gt);

        (*numSuperpixels) = relabelSuperpixels(labels, 8);
        score = computeCompactness(labels);
        iftDestroyImage(&labels);

        return score;
    }

    if (args.metric == 6)
    {
#ifdef DEBUG
        printf("Connectivity \n"); // Added GT - Need to change metric function, will be a problem cause it uses the relabelSuperpixel for the score - free(): invalid pointer
#endif
        iftImage *labels, *gt = NULL;
        double score = 0;
        char fileName[255], labels_path[255], gt_path[255];

        getImageName(image_name, fileName);
        readFileInDir(fileName, args.label_path, args.label_ext, labels_path);
        labels = iftReadImageByExt(labels_path);
        
        (*numSuperpixels) = getNumSuperpixels(labels);
        
        if (args.removeColor != -1){
            sprintf(gt_path, "%s/%s", args.gt_path, image_name); // Used for mask
            gt = iftReadImageByExt(gt_path); // Used for mask
            removeSuperpixelsByGTColor(labels, gt, args.removeColor); // Used for mask
        }

        /*if (args.removeSize != -1)
        {
            removeSuperpixelsBySize(labels, gt, args.removeSize); // Used for mask
        }*/

        if(gt != NULL) iftDestroyImage(&gt);
        score = (double)relabelSuperpixels(labels, 8);

        if (args.saveLabels != NULL)
        {
            char *save_path = (char *)malloc(255 * sizeof(char));
            readFileInDir(fileName, args.saveLabels, "pgm", save_path);
            iftWriteImageByExt(labels, save_path);
            free(save_path);
        }
        iftDestroyImage(&labels);
        return score;
    }

    if (args.metric == 7)
    {
#ifdef DEBUG
        printf("Enforce superpixels' number \n"); // Added GT - I assume it will result in an error but testing is needed
#endif
        iftImage *labels, *image, *gt = NULL;
        double score = 0;
        char fileName[255], labels_path[255], gt_path[255];

        getImageName(image_name, fileName);
        readFileInDir(fileName, args.label_path, args.label_ext, labels_path);
        
        labels = iftReadImageByExt(labels_path);
        
        if (args.removeColor != -1){
            sprintf(gt_path, "%s/%s", args.gt_path, image_name); // Used for mask
            gt = iftReadImageByExt(gt_path); // Used for mask
            removeSuperpixelsByGTColor(labels, gt, args.removeColor); // Used for mask
        }
        
        /*if (args.removeSize != -1)
        {
            removeSuperpixelsBySize(labels, gt, args.removeSize); // Used for mask
        }*/

        if(gt != NULL) iftDestroyImage(&gt);
        (*numSuperpixels) = relabelSuperpixels(labels, 8);

        if (args.saveLabels != NULL)
        {
            char *save_path = (char *)malloc(255 * sizeof(char));
            readFileInDir(fileName, args.saveLabels, "pgm", save_path);
            image = readRGBImage(save_path);

            if (image->xsize != labels->xsize || image->ysize != labels->ysize || image->zsize != labels->zsize)
                printError("eval", "Image and labels must have the same size");

            score = enforceNumSuperpixel(labels, image, (*numSuperpixels));
            iftDestroyImage(&image);

            if (score != (*numSuperpixels))
                printError("eval", "Computing the wrong number of superpixels.");
            iftWriteImageByExt(labels, save_path);
            free(save_path);
        }
        else
            score = 0;

        iftDestroyImage(&labels);
        return score;
    }

    if (args.metric == 8)
    {
#ifdef DEBUG
        printf("Qualitative \n"); // Not used
#endif

        iftImage *labels, *gt;
        char fileName[255], labels_path[255], gt_path[255], save_path[255];
       
        char *basename, ext[10], filename[255];
        // get file extension and image name
        sprintf(ext, "%s", iftFileExt(labels_path));
        basename = iftFilename(labels_path, ext);
        sprintf(filename, "%s/%s.png", gt_path, basename);

        labels = iftReadImageByExt(labels_path);
        gt = iftReadImageByExt(filename);

        // ideally, this should be in args
        iftColor ignoreColorGt;
        ignoreColorGt.val[0] = 16;
        ignoreColorGt.val[1] = 128;
        ignoreColorGt.val[2] = 128;

        recreateLabels(labels, gt, ignoreColorGt); // Used for mask

        sprintf(filename, "%s/%s%s", save_path, basename, ext);
        iftWriteImageByExt(labels, filename);

        iftDestroyImage(&labels);
        iftDestroyImage(&gt);
        iftFree(basename);

        /*
        iftImage *labels1, *labels2, *image;
        double score = 0;
        int maxLabel;
        char fileName[255], labels_path1[255], img_path[255], save_path[255];

        getImageName(image_name, fileName);

        readFileInDir(fileName, args.label_path, args.label_ext, labels_path1);
        labels1 = iftReadImageByExt(labels_path1);
        maxLabel = relabelSuperpixels(labels1, labels1->ysize, labels1->xsize, 8);

        if(args.label_path2 != NULL){
            char labels_path2[255];
            readFileInDir(fileName, args.label_path2, args.label_ext, labels_path2);
            labels2 = iftReadImageByExt(labels_path2);
            maxLabel = relabelSuperpixels(labels2, labels2->ysize, labels2->xsize, 8);
        }else labels2 = NULL;

        sprintf(img_path, "%s/%s", args.img_path, image_name);
        image = iftReadImageByExt(img_path);
        sprintf(save_path, "%s/%s.png", args.saveLabels, fileName);
        qualitative(image, labels1, labels2, save_path, args);

        iftDestroyImage(&image);
        iftDestroyImage(&labels1);
        if(args.label_path2 != NULL) iftDestroyImage(&labels2);
        return score;
        */
    }

    if (args.metric == 9)
    {
#ifdef DEBUG
        printf("Regularity \n");
#endif
        iftImage *labels, *gt = NULL, *image;
        double score = 0;
        char fileName[255], labels_path[255], gt_path[255];

        getImageName(image_name, fileName);
        readFileInDir(fileName, args.label_path, args.label_ext, labels_path);
        
        labels = iftReadImageByExt(labels_path);
        
        if (args.removeColor != -1){
            sprintf(gt_path, "%s/%s", args.img_path, image_name); // Used for mask
            gt = iftReadImageByExt(gt_path); // Used for mask
            removeSuperpixelsByGTColor(labels, gt, args.removeColor); // Used for mask
        }
        
        /*if (args.removeSize != -1)
        {
            removeSuperpixelsBySize(labels, gt, args.removeSize); // Used for mask
        }*/

        if(gt != NULL) iftDestroyImage(&gt);
        (*numSuperpixels) = relabelSuperpixels(labels, 8);
        score = computeRegularity(labels);
        iftDestroyImage(&labels);
        return score;
    }

    if (args.metric == 10)
    {
#ifdef DEBUG
        printf("Count small superpixels \n");
#endif
        iftImage *labels, *gt = NULL, *image;
        double score = 0;
        char fileName[255], labels_path[255], gt_path[255], save_path[255];

        getImageName(image_name, fileName);
        readFileInDir(fileName, args.label_path, args.label_ext, labels_path);
        
        labels = iftReadImageByExt(labels_path);

        if (args.removeColor != -1){
            sprintf(gt_path, "%s/%s", args.gt_path, image_name); // Used for mask
            gt = iftReadImageByExt(gt_path); // Used for mask
            removeSuperpixelsByGTColor(labels, gt, args.removeColor); // Used for mask
        }
        
        /*if (args.removeSize != -1)
        {
            removeSuperpixelsBySize(labels, gt, args.removeSize); // Used for mask
        }*/

        (*numSuperpixels) = relabelSuperpixels(labels, 8);
        score = numSpxSmallerThan(labels, args.removeSize);

        if (args.saveLabels != NULL)
        {
            char *save_path = (char *)malloc(255 * sizeof(char));
            char img_path[255];
            readFileInDir(fileName, args.saveLabels, "pgm", save_path);

            iftDestroyImage(&labels);
            labels = iftReadImageByExt(labels_path);
            sprintf(img_path, "%s/%s", args.img_path, image_name);
            image = readRGBImage(img_path);

            if (image->xsize != labels->xsize || image->ysize != labels->ysize || image->zsize != labels->zsize)
                printError("eval", "Image and labels must have the same size");

            // ideally, this should be in args
            iftColor ignoreColorGt;
            ignoreColorGt.val[0] = 16;
            ignoreColorGt.val[1] = 128;
            ignoreColorGt.val[2] = 128;

            mergeSpxBasedOnSize(labels, image, args.removeSize, gt, ignoreColorGt, NULL);
            iftDestroyImage(&image);
            iftWriteImageByExt(labels, save_path);
            free(save_path);
        }

        if(gt != NULL) iftDestroyImage(&gt);
        iftDestroyImage(&labels);
        return score;
    }
    return -1;
}

void getImageName(char *fullImgPath, char *fileName)
{
    char *imgName;
    int length, endFileName;

    imgName = fullImgPath;
    length = strlen(imgName);

    while (imgName[length] != '.')
    {
        length--;
    }
    endFileName = length;
    while (imgName[length] != '/' && length > 0)
    {
        length--;
    }
    if (imgName[length] == '/')
        length++;

    imgName += length;
    strncpy(fileName, imgName, endFileName - length);
    fileName[endFileName - length] = '\0';
}

// return true if the file specified
// by the filename exists
bool file_exists(const char *filename)
{
    struct stat buffer;
    return (stat(filename, &buffer) == 0 && (buffer.st_mode & S_IFMT) == S_IFREG) ? true : false;
}

void runDirectory(Args args)
{
    // determine mode : file or path
    struct stat sb;
    double score_path = 0;
    int numSuperpixels = 0;

    if (stat(args.img_path, &sb) == -1)
    {
        perror("stat");
        exit(EXIT_SUCCESS);
    }

    int type;
    switch (sb.st_mode & S_IFMT)
    {
    case S_IFDIR:
        printf("Directory processing: ");
        type = 0;
        break;
    case S_IFREG:
        printf("Single file processing\n");
        type = 1;
        break;
    default:
        type = -1;
        break;
    }

    if (type == -1)
        exit(EXIT_SUCCESS);
    else if (type == 1)
    {
        double score_img = 0;
        char fileName[255];

        getImageName(args.img_path, fileName);
        score_img = eval(args.img_path, args, &numSuperpixels);

        // ************************
        if (args.metric != 8)
        {
            if (args.dLogFile != NULL)
            {
                bool file_exist = file_exists(args.dLogFile);
                FILE *fp = fopen(args.dLogFile, "a+");

                if (!file_exist)
                {
                    if (args.metric < 6)
                        fprintf(fp, "Image Superpixels Score\n");
                    else
                    {
                        if (args.metric == 6)
                            fprintf(fp, "Image Superpixels ConnectedSpx\n");
                        else
                            fprintf(fp, "Image DesiredSpx Superpixels\n");
                    }
                }

                if (args.metric == 7)
                    fprintf(fp, "%s %d %d\n", fileName, args.k, numSuperpixels);
                else
                    fprintf(fp, "%s %d %.5f\n", fileName, numSuperpixels, score_img);
                fclose(fp);
            }

            if (args.metric < 6)
                printf("Score: %.5f , superpixels: %d \n", score_img, numSuperpixels);
            else
            {
                if (args.metric == 6)
                    printf("Superpixels: %d , Connected superpixels: %.5f \n", numSuperpixels, score_img);
                else
                    printf("Desired superpixels: %d , Generated superpixels: %d \n", args.k, numSuperpixels);
            }
        }
    }
    else if (type == 0)
    {
        // get file list
        struct dirent **namelist;
        int n;

        if (args.img_path != NULL)
            n = scandir(args.img_path, &namelist, &filterDir, alphasort);
        else
            n = scandir(args.label_path, &namelist, &filterDir, alphasort);

        if (n == -1)
        {
            printf("No images found.\n");
            perror("scandir");
            exit(EXIT_FAILURE);
        }

        if (n == 0)
        {
            printf("No images found.\n");
            exit(EXIT_SUCCESS);
        }
        else
            printf("%d Images found.\n", n);

        // process file list

        int numImages = n;
        int sum_num_superpixel = 0;

        while (n--)
        {
            char fileName[255];
            double score_img = 0;

            // get image name
            getImageName(namelist[n]->d_name, fileName);

            // ********
            score_img = eval(namelist[n]->d_name, args, &numSuperpixels);
            // ********
            sum_num_superpixel += numSuperpixels;
            score_path += score_img;

            if (args.dLogFile != NULL && args.metric != 8)
            {
                bool file_exist = file_exists(args.dLogFile);
                FILE *fp = fopen(args.dLogFile, "a+");

                if (!file_exist)
                {
                    if (args.metric < 6)
                        fprintf(fp, "Image Superpixels Score\n");
                    else
                    {
                        if (args.metric == 6)
                            fprintf(fp, "Image Superpixels ConnectedSpx\n");
                        else
                            fprintf(fp, "Image DesiredSpx Superpixels\n");
                    }
                }
                if (args.metric == 7)
                    fprintf(fp, "%s %d %d\n", fileName, args.k, numSuperpixels);
                else
                    fprintf(fp, "%s %d %.5lf\n", fileName, numSuperpixels, score_img);
                fclose(fp);
            }

            free(namelist[n]);
        }

        free(namelist);

        if (args.logFile != NULL)
        {
            bool file_exist = file_exists(args.logFile);
            FILE *fp = fopen(args.logFile, "a+");

            if (!file_exist)
            {
                if (args.metric < 6)
                    fprintf(fp, "Superpixels Score\n");
                else
                {
                    if (args.metric == 6)
                        fprintf(fp, "Superpixels ConnectedSpx\n");
                    else
                        fprintf(fp, "DesiredSpx Superpixels\n");
                }
            }

            if (args.metric == 7)
                fprintf(fp, "%d %.5f\n", args.k, (double)sum_num_superpixel / (double)numImages);
            else
                fprintf(fp, "%.5f %.5f\n", (double)sum_num_superpixel / (double)numImages, score_path / (double)numImages);
            fclose(fp);
        }
    }
}


void runOvlayDir(char *orig_path, char *labels_path, char *gt_path, char *save_path)
{
    if (!iftDirExists(labels_path))
        iftError("Labels directory not found", __func__);
    if (!iftDirExists(gt_path))
        iftError("Ground truth directory not found", __func__);
    if (!iftDirExists(orig_path))
        iftError("Images directory not found", __func__);
    if (!iftDirExists(save_path))
        iftMakeDir(save_path);

    iftColor spxColor, gtColor, overlapColor;
    // YCbCr = (210, 16, 146)
    spxColor.val[0] = 210;
    spxColor.val[1] = 16;
    spxColor.val[2] = 146;

    // (82, 90, 240);
    gtColor.val[0] = 82;
    gtColor.val[1] = 90;
    gtColor.val[2] = 240;

    // YCbCr =  (41, 240, 110);
    overlapColor.val[0] = 41;
    overlapColor.val[1] = 240;
    overlapColor.val[2] = 110;

    char ext_labels[10];
    iftFileSet *labels_files = iftLoadFileSetFromDir(labels_path, 1);
    sprintf(ext_labels, "%s", iftFileExt(labels_files->files[0]->path));
    iftDestroyFileSet(&labels_files);
    iftFileSet *orig_files = iftLoadFileSetFromDir(orig_path, 1);

    for (int i = 0; i < orig_files->n; i++)
    {
        char *basename, ext[10], filename[200];

        // get file extension and image name
        sprintf(ext, "%s", iftFileExt(orig_files->files[i]->path));
        basename = iftFilename(orig_files->files[i]->path, ext);
        iftImage *orig_img = iftReadImageByExt(orig_files->files[i]->path);

        sprintf(filename, "%s/%s%s", labels_path, basename, ext_labels);
        iftImage *labels_img = iftReadImageByExt(filename);

        sprintf(filename, "%s/%s.png", gt_path, basename);
        iftImage *gt_img = iftReadImageByExt(filename);

        sprintf(filename, "%s/%s.png", save_path, basename);
        iftImage *save_img = ovlayBorders(orig_img, labels_img, gt_img, 1, spxColor, gtColor, overlapColor);
        iftWriteImageByExt(save_img, filename);

        iftDestroyImage(&labels_img);
        iftDestroyImage(&gt_img);
        iftDestroyImage(&orig_img);
        iftDestroyImage(&save_img);
        iftFree(basename);
    }
    iftDestroyFileSet(&orig_files);
}

int main(int argc, char *argv[])
{

#ifdef DEBUG
    printf("DEGUB true\n");
#endif

    Args args;
    if (initArgs(&args, argc, argv))
        runDirectory(args);
    else
        usage();

    return 0;
}

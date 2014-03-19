

#include <stdio.h>
#include "CCNLiteInterface.h"
#include "ccnb.c"
#include "ccn-lite-ccnb2xml-java.c"

// #ifdef __APPLE__
#include "open_memstream.h"
// #endif

JNIEXPORT jstring JNICALL
Java_CCNLiteInterface_ccnbToXml(JNIEnv *env, jobject obj, jbyteArray binaryInterest)
{
    jbyte* jInterestData = (*env)->GetByteArrayElements(env, binaryInterest, NULL);
    jsize len = (*env)->GetArrayLength(env, binaryInterest);

    unsigned char interestData[8*1024], *buf = interestData;
    bzero(interestData, 8*1024);
    memcpy(interestData, jInterestData, len);


    // For now the result is written to a file (stream buffer does crash for some reason)
    remove("c_mxl.txt");
    FILE *file = fopen("c_xml.txt", "w");
    if (file == NULL)
    {
        printf("Error opening file!\n");
        exit(1);
    }

    // Writes the ccnb interest as xml to the stream
    ccnb2xml(0, interestData, &buf, &len, NULL, file, true);

    fflush(file);
    close(file);


    // Read the file again and convert it into a jstring
    file = fopen("c_xml.txt", "r");
    char *buffer;
    long file_size;
    fseek(file, 0L, SEEK_END);
    file_size = ftell(file);
    rewind(file);
    buffer = malloc(file_size + 1);
    jstring java_string;
    if (buffer != NULL )
    {
        fread(buffer, file_size, 1, file);
        buffer[file_size] = '\0';
        close(file);

        java_string = (*env)->NewStringUTF(env, buffer);

        free(buffer);
        buffer = NULL;
    } else {
        fprintf(stderr, "Error when allocating buffer");
        exit(3);
    }

    if (file != NULL) fclose(file);
    // (*env)->ReleaseByteArrayElements(env, binaryInterest, jInterestData, 0);
    // TODO remove temp file
    // remove("c_mxl.txt");

    // fprintf("'%s'", java_string);
    // printf("======\n%s======\n", (*env)->GetStringUTFChars(env, java_string, 0));


    return java_string;
}

// void
// javaStringArrayToCArray(JNIEnv *env,
//                         jobjectArray javaArray) {

// }

JNIEXPORT jbyteArray JNICALL
Java_CCNLiteInterface_mkBinaryContent(JNIEnv *env,
                                      jobject obj,
                                      jobjectArray nameComponentStringArray,
                                      jbyteArray j_content)
{

    char *components[CCNL_MAX_NAME_COMP], *component;
    int componentCount;
    unsigned char content_data[8*1024], *buf;

    unsigned char out[65*1024];
    unsigned char *publisher = -1;
    int i = 0, binary_content_len, publisher_len = 0;
    char *prefix[CCNL_MAX_NAME_COMP], *cp;
    char *private_key_path = 0;

    jsize content_len;
    jbyte* jbyte_content;

    // Convert content from java byte array to c byte array
    jbyte_content = (*env)->GetByteArrayElements(env, j_content, NULL);
    content_len = (*env)->GetArrayLength(env, j_content);
    memcpy(content_data, jbyte_content, content_len);
    buf = jbyte_content;

    // get component strings from java string arrray
    componentCount = (*env)->GetArrayLength(env, nameComponentStringArray);
    for(int i = 0; i < componentCount; i++) {
        jstring cmpString = (jstring) (*env)->GetObjectArrayElement(env,
                                                                    nameComponentStringArray,
                                                                    i);
        component = (*env)->GetStringUTFChars(env, cmpString, 0);
        components[i] = malloc(strlen(component) + 1);
        strcpy(components[i], component);
    }




    binary_content_len = mkContent(components,
                                   componentCount,
                                   publisher,
                                   publisher_len,
                                   private_key_path,
                                   content_data,
                                   content_len,
                                   out);
    // binary_content_len = 0;

    jbyteArray jbyte_ccnb_content = (*env)->NewByteArray(env, binary_content_len);
    jbyte *bytes = (*env)->GetByteArrayElements(env, jbyte_ccnb_content, 0);
    memcpy(bytes, out, binary_content_len);
    (*env)->SetByteArrayRegion(env, jbyte_ccnb_content, 0, binary_content_len, bytes );

    for(int i = 0; i < componentCount; i++) {
        if(components[i] != NULL) {
            free(components[i]);
            components[i] = NULL;
        }
    }



    // (*env)->ReleaseStringUTFChars(env, j_name, content_name);
    // (*env)->ReleaseByteArrayElements(env, j_content, jbyte_ccnb_content, 0);

    return jbyte_ccnb_content;
}

JNIEXPORT jbyteArray JNICALL
Java_CCNLiteInterface_mkBinaryInterest(JNIEnv *env,
                                       jobject obj,
                                       jobjectArray nameComponentStringArray)
{

    printf("start\n");

    char *components[CCNL_MAX_NAME_COMP], *component;
    int componentCount;
    unsigned char out[8*1024], *buf = out;
    char *minSuffix = 0, *maxSuffix = 0, *scope;
    unsigned char *digest = 0, *publisher = 0;
    char *fname = 0;
    int str_length = 0, f, len, opt;
    int dlen = 0, plen = 0;
    uint32_t nonce;

    // get component strings from java string arrray
    componentCount = (*env)->GetArrayLength(env, nameComponentStringArray);
    for(int i = 0; i < componentCount; i++) {
        jstring cmpString = (jstring) (*env)->GetObjectArrayElement(env,
                                                                    nameComponentStringArray,
                                                                    i);
        component = (*env)->GetStringUTFChars(env, cmpString, 0);
        components[i] = malloc(strlen(component) + 1);
        strcpy(components[i], component);
    }
    // init the nonce
    time((time_t*) &nonce);

    // mk the ccnb interest
    len = mkInterest(components, componentCount,
             minSuffix, maxSuffix,
             digest, dlen,
             publisher, plen,
             scope, &nonce,
             out);

    jbyteArray javaByteArray = (*env)->NewByteArray(env, len);
    jbyte *bytes = (*env)->GetByteArrayElements(env, javaByteArray, 0);
    memcpy(bytes, out, len);
    (*env)->SetByteArrayRegion(env, javaByteArray, 0, len, bytes );

    for(int i = 0; i < componentCount; i++) {
        if(components[i] != NULL) {
            free(components[i]);
            components[i] = NULL;
        }
    }

    return javaByteArray;
}

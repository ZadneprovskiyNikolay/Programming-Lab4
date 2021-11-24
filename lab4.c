#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const int MAX_FILEPATH = 260;

struct CmdArgs { 
    char filepath[260];
    char prop_name[4];        
    char* prop_value;
    int show, set, get;
};

struct String {
    char* str;
    size_t size;    
};

void init_String(struct String* str, int size) {
    str->size = size;
    str->str = (char*) malloc(size * sizeof(char));
}

void extend_String(struct String* str, int size) {
    if (size > str->size)
        str->str = (char*) realloc(str->str, size * sizeof(char));
}

// Returns 1 for valid arguments, 0 otherwise.
int parse_cmdargs(int argc, char* argv[], struct CmdArgs* cmdargs) {     
    if (argc < 3) {
        printf("You didn't provide enough arguments.\n");
        return 0;
    }

    // Filename.
    if (*(argv[1] + 2) == 'f') {  
        strncpy(cmdargs->filepath, argv[1] + 11, MAX_FILEPATH);        
    } 
    else {
        printf("Incorrect first argument, format: --filepath=file_name.mp3\n");
        return 0;
    }

    // Show.
    if (*(argv[2] + 2) == 's' && *(argv[2] + 3) == 'h') {
        cmdargs->show = 1;
    }    
    // Get.
    else if (*(argv[2] + 2) == 'g') {
        cmdargs->get = 1;
        strncpy(cmdargs->prop_name, argv[2] + 6, 4);        
    }
    // Set.    
    else if (*(argv[2] + 2) == 's') {  
        if (argc < 4 || strlen(argv[3]) - 8 == 0) {
            printf("You didn't provide value to set.");
            return 0;
        }        
        cmdargs->show = 1;
        strncpy(cmdargs->prop_name, argv[2] + 6, 4);        
        int prop_value_length = strlen(argv[3]) - 8;
        cmdargs->prop_value = (char*) malloc(prop_value_length * sizeof(char));
        strncpy(cmdargs->prop_value, argv[3] + 8, prop_value_length);        
    } 
    else {
        printf("Incorrect second argument.\n");
        return 0;
    }    

    return 1;
}

int bytes_to_size(char* buffer) {
    int res = 0;
    for (int x = 0; x < 4; x++) {
        res += buffer[x] << (7 * (3 - x));        
    }
    return res;
}

void print_metadata(char* input_filename, char name_to_print[4]) { 
    FILE* input_file = fopen(input_filename, "rb");
    if (input_file == NULL) {
        printf("Couldn't open input file.\n");
        exit(EXIT_FAILURE);
    }

    // Parse tag header.
    int tag_size;
    char header[10];
    struct String buffer;
    init_String(&buffer, 100);
    fread(header, sizeof(header), 1, input_file);

    tag_size = bytes_to_size(header + 6);
    
    for (int frame_idx = 0, bytes_read = 0;; frame_idx++) {
        // Read frame header.
        fread(header, sizeof(header), 1, input_file);
        bytes_read += 10;
        if (bytes_read > tag_size) break;
        int frame_size = bytes_to_size(header + 4);

        if (frame_size == 0) continue;

        // Read frame name.
        char name[5];
        name[4] = '\0';
        for (int x = 0; x < 4; x++) 
            name[x] = header[x];

        // Read frame data.
        extend_String(&buffer, frame_size);
        fread(buffer.str, frame_size, 1, input_file);
        bytes_read += frame_size;       
        if (bytes_read > tag_size) break;

        // Print frame.
        if ((name_to_print == NULL) || (name_to_print != NULL && !strcmp(name, name_to_print))) {
            printf("%s:\n", name, buffer.str);
            for (int x = 0; x < frame_size; x++) {
                printf("%c", buffer.str[x]);
            }
            printf("\n\n");
        }
    }    

    fclose(input_file);
}

int main(int argc, char* argv[]) {    
    // Parse arguments.
    struct CmdArgs cmdargs = { NULL, NULL, NULL, NULL, 0, 0 };
    if (!parse_cmdargs(argc, argv, &cmdargs)) {
        exit(EXIT_FAILURE);
    }

    if (cmdargs.show || cmdargs.get) {
        FILE* input_file = fopen(cmdargs.filepath, "rb");

        if (cmdargs.show) { // Show.
            print_metadata(cmdargs.filepath, NULL);
        }
        else { // Get.
            print_metadata(cmdargs.filepath, cmdargs.prop_name);
        }
    }

    return 0;
}
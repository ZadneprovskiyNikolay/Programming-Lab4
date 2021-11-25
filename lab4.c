#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

const int MAX_FILEPATH = 260;

struct CmdArgs { 
    char* prop_value;
    int show, set, get;
    char prop_name[5];        
    char filepath[MAX_FILEPATH + 1];
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
        int filepath_len = strlen(argv[1]) - 11;        
        strncpy(cmdargs->filepath, argv[1] + 11, MAX_FILEPATH);  
        cmdargs->filepath[filepath_len] = '\0';
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
        cmdargs->set = 1;
        strncpy(cmdargs->prop_name, argv[2] + 6, 4);        
        int prop_value_length = strlen(argv[3]);
        cmdargs->prop_value = (char*) malloc(prop_value_length * sizeof(char));
        strncpy(cmdargs->prop_value, argv[3] + 8, prop_value_length);        
    } 
    else {
        printf("Incorrect second argument.\n");
        return 0;
    }    

    return 1;
}

unsigned int bytes_to_size(char dest[4]) {
    unsigned int res = 0;
    for (int x = 0; x < 4; x++) {
        res += dest[x] << (8 * (3 - x));
    }    
    return res;
}

unsigned int size_to_bytes(unsigned int size, char dest[4]) {    
    for (int x = 0; x < 4; x++) {
        dest[x] = size >> (8 * (3 - x));
    }        
}

void size_to_bytes_tag_header(unsigned int size, char dest[4]) {
    for (int x = 0; x < 4; x++) {
        dest[x] = size >> (7 * (3 - x)) & 0x7F;
    }
}

unsigned int bytes_to_size_tag_header(char* buffer) {
    int res = 0;
    for (int x = 0; x < 4; x++) {
        res += buffer[x] << (7 * (3 - x));        
    }   
    return res;
}

void set_field(FILE* input_file, struct CmdArgs args, int field_present, int frame_size, int first_empty_frame) {
    FILE* output_file = fopen("new_file.mp3", "wb+");

    // Number of bytes to copy before new frame(-10 because I write new tag header).
    long cpy_bytes_num;
    if (field_present)
        cpy_bytes_num = ftell(input_file); 
    else
        cpy_bytes_num = first_empty_frame;
    cpy_bytes_num -= 10;

    // Recompute tag size and write it with header.
    char header[10];
    fseek(input_file, 0, SEEK_SET);
    fread(header, 10, 1, input_file); // Read first 6 bytes from header.
    int tag_size = bytes_to_size_tag_header(header + 6);
    if (field_present)
        tag_size = tag_size - frame_size + strlen(args.prop_value);
    else
        tag_size += 10 + strlen(args.prop_value);
    size_to_bytes_tag_header(tag_size, &header[6]);        
    int how_many_wrote = fwrite(header, 10, 1, output_file);
    if (how_many_wrote != 1) {
        printf("Wrote incorrect header(%d).\n", how_many_wrote);
        exit(EXIT_FAILURE);
    };                

    // Write all data till current frame to the new file.
    int ch;
    for (long x = 0; x < cpy_bytes_num; x++) {
        ch = fgetc(input_file);
        fputc(ch, output_file);
    }

    // Write header of the new frame.
    strcpy(header, args.prop_name);
    size_to_bytes(strlen(args.prop_value) + 1, &header[4]); // Plus 1 zero byte after header.    
    header[8] = 0x00; // Set flags to 0.
    header[9] = 0x00;
    fwrite(header, 10, 1, output_file);    
    fputc(0, output_file); // Write zero byte after header.
    fwrite(args.prop_value, strlen(args.prop_value), 1, output_file);
    
    // Write remaining data to file.
    if (field_present)
        fseek(input_file, frame_size + 10, SEEK_CUR); // Put file pointer to the end of the replaced frame.
    else
        fseek(input_file, first_empty_frame, SEEK_SET); // Put file pointer to the end of the replaced frame.
    while ((ch = fgetc(input_file)) != EOF)
        fputc(ch, output_file);

    // Delete original file. Rename created file.
    fclose(input_file);
    fclose(output_file);    
    remove(args.filepath);
    usleep(1000 * 10);
    if (rename("new_file.mp3", args.filepath)) {
        printf("Couldn't rename file, error: %s\n", strerror(errno));        
    } 
}

void process_cmds(struct CmdArgs args) { 
    int field_present = 0; // For set command.
    int saved_frame_size = 0; // Size of field to edit.
    long first_empty_frame = -1; // Bytes padding from start of file to the first empty frame.

    if (args.set)
        remove("new_file.mp3");

    FILE* input_file = fopen(args.filepath, "rb");
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

    tag_size = bytes_to_size_tag_header(header + 6);
    
    for (int frame_idx = 0, bytes_read = 0;; frame_idx++) {
        // Read frame header.
        fread(header, sizeof(header), 1, input_file);
        bytes_read += 10;
        if (bytes_read > tag_size) break;
        unsigned int frame_size = bytes_to_size(header + 4);

        if (frame_size == 0) {
            if (first_empty_frame == -1) {
                first_empty_frame = ftell(input_file) - sizeof(header);
            }
            fseek(input_file, 1, SEEK_CUR); // Minimum one bytes size of frame.
            bytes_read++;
            continue;
        }

        // Read frame name.
        char frame_name[5];
        frame_name[4] = '\0';
        for (int x = 0; x < 4; x++) 
            frame_name[x] = header[x];

        // Read frame data.
        extend_String(&buffer, frame_size);
        fread(buffer.str, frame_size, 1, input_file);
        bytes_read += frame_size;    
        if (bytes_read > tag_size) break;

        // Print frame if needed.
        if (args.show || (args.get && !strcmp(frame_name, args.prop_name))) {
            printf("%s:\n", frame_name, buffer.str);
            for (int x = 0; x < frame_size; x++) {
                printf("%c", buffer.str[x]);
            }
            printf("\n\n");
        }        
        else if (args.set && !strcmp(frame_name, args.prop_name)) { 
            fseek(input_file, -(10 + frame_size), SEEK_CUR); // Move file pointer to the begining of the frame.          
            field_present = 1;
            saved_frame_size = frame_size;
            break;
        }
    }    

    if (args.set) {
        set_field(input_file, args, field_present, saved_frame_size, first_empty_frame);
    }

    fclose(input_file);
}

int main(int argc, char* argv[]) {    
    struct CmdArgs cmdargs = { NULL, 0, 0, 0, { 0, 0, 0, 0, '\0' } };
    if (!parse_cmdargs(argc, argv, &cmdargs)) {
        exit(EXIT_FAILURE);
    }

    process_cmds(cmdargs);

    return 0;
}
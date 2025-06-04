// Después ejecutar este programa que hace lo siguiente:
// - Escribe un superblock válido
// - Inicailiza tabla de inodos
// - Configura el inodo 0 como directorio raíz
// - Reserva un bloque de datos para sus entradas (aunque estén vacío).
// --------------------------------------------------------------------
// Operaciones:
//     ./miniEXT2_init_Format_FS  // prepara imagen
//     mkdir -p miFS              // crea carpeta
//     make mount                 // ./miniEXT2 imagen miFS
//     make debug                 // ./miniEXT2 -d imagen miFS
//     make umount                // fusermount -u miFS
//     make clean                 // -rmdir miFS
// --------------------------------------------------------------------
#include "struct.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <archivo_imagen>\n", argv[0]);
        return 1;
    }
    const char *imagen = argv[1];

    // Abrir y medir tamaño de la imagen
    int fd = open(imagen, O_RDWR);
    if (fd < 0) {
        perror("open imagen");
        return 1;
    }
    struct stat st;
    if (fstat(fd, &st) < 0) {
        perror("fstat");
        close(fd);
        return 1;
    }
    off_t img_size = st.st_size;
    uint32_t total_blocks = img_size / BLOCK_SIZE;

    FILE *f = fdopen(fd, "r+b");
    if (!f) {
        perror("fdopen");
        close(fd);
        return 1;
    }

    // Preparamos el superbloque
    struct superblock sb = {0};
    sb.total_blocks     = NUM_BLOCKS;
    sb.block_size       = BLOCK_SIZE;
    sb.total_inodes     = NUM_INODES;
    sb.free_blocks      = NUM_BLOCKS - FIRST_DATA_BLOCK - 1; // -1 por superbloque
    sb.free_inodes      = NUM_INODES - 1;                    // inodo 0 usado por root
    sb.first_data_block = FIRST_DATA_BLOCK;
    strncpy(sb.fs_name, "MiniEXT2", sizeof(sb.fs_name));

    // Escribimos el superbloque al inicio
    fseek(f, 0, SEEK_SET);
    fwrite(&sb, sizeof(sb), 1, f);

    // Tabla de inodos: bloque 5
    off_t inode_table_start = BLOCK_SIZE * INODE_TABLE_BLK;
    fseek(f, inode_table_start, SEEK_SET);

    // Inodo 0: directorio raíz
    struct inode root = {0};
    root.uid         = 0;
    root.gid         = 0;
    root.mode        = 0040755; // S_IFDIR | 0755
    
    root.size        = 2 * sizeof(struct dir_entry);
    root.blocks      = 1;

    root.direct[0]   = FIRST_DATA_BLOCK; // Primer bloque de datos
    root.atime       = time(NULL);
    root.ctime       = time(NULL);
    root.mtime       = time(NULL);
    root.links_count = 2;
    fwrite(&root, sizeof(root), 1, f);


    // Rellenamos el resto de inodos con ceros
    struct inode empty = {0};
    for (int i = 1; i < NUM_INODES; i++) {
        fwrite(&empty, sizeof(empty), 1, f);
    }

    // Inicializar bloque de datos del root con "." y ".."
    off_t data_block_offset = (off_t)FIRST_DATA_BLOCK * BLOCK_SIZE;
    fseek(f, data_block_offset, SEEK_SET);
    int entries_per_block = BLOCK_SIZE / sizeof(struct dir_entry);
    struct dir_entry *block = calloc(entries_per_block, sizeof(*block));
    if (!block) {
        perror("calloc");
        fclose(f);
        return 1;
    }
    //block[0].inode = 0; strcpy(block[0].name, ".");
    //block[1].inode = 0; strcpy(block[1].name, "..");
    fwrite(block, BLOCK_SIZE, 1, f);

    free(block);
    fclose(f);
    printf("Tabla de inodos escrita en %ld.\n", inode_table_start);
    printf("Bloque de datos del root inicializado en bloque %ld.\n", data_block_offset);
    printf("✅ FS inicializado: %u bloques, %d inodos, datos desde bloque %ld.\n",
           NUM_BLOCKS, NUM_INODES, FIRST_DATA_BLOCK);
    return 0;
}
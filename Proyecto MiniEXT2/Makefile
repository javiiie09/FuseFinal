# Nombre del ejecutable FUSE y del formateador
EXEC    := miniEXT2
MKIMG   := formatEXT2

# Parámetros de imagen
IMG       := imagen.img
IMG_SIZE  := 10  # Tamaño de la imagen en MiB

# Punto de montaje
MNTDIR    := mount_point

# Fuentes
SRC_FS    := miniEXT2.c
SRC_MKIMG := miniEXT2_init_Format_FS.c

# Flags para compilación
CC        := gcc
CFLAGS    := -g -Wall -Wextra -D_FILE_OFFSET_BITS=64 -lfuse -pthread
LDFLAGS   := -D_FILE_OFFSET_BITS=64 -lfuse -pthread

.PHONY: all format mount debug umount clean test

all: $(EXEC) $(MKIMG)

# 1) Compilar el sistema de ficheros
$(EXEC): $(SRC_FS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# 2) Compilar el formateador de imagen
$(MKIMG): $(SRC_MKIMG)
	$(CC) -o $@ $<

# 3) Crear una imagen vacía y formatearla
format: $(MKIMG)
	@echo "==> Creando imagen de $(IMG_SIZE)MiB con $(INODES) inodos..."
	rm -f $(IMG)
	dd if=/dev/zero of=$(IMG) bs=1M count=$(IMG_SIZE) status=progress
	./$(MKIMG) $(IMG)

# 4) Montar el FS (foreground)  
mount: $(EXEC)
	@echo "==> Montando el sistema de ficheros..."
	@mkdir -p $(MNTDIR)
	@echo "==> Montando $(IMG) en $(MNTDIR)..."
	./$(EXEC) $(IMG) $(MNTDIR)

# 5) Modo depuración (-d)
debug: $(EXEC)
	@mkdir -p $(MNTDIR)
	@echo "==> Arrancando en modo debug..."
	./$(EXEC) $(IMG) $(MNTDIR) -f -d

# 6) Desmontar
umount:
	@echo "==> Desmontando $(MNTDIR)..."
	fusermount -u $(MNTDIR)

# 7) Limpiar binarios y directorio (si está vacío)
clean:
	@echo "==> Limpiando..."
	rm -f $(EXEC) $(MKIMG) $(IMG)
	rm -r $(MNTDIR)

# 8) Ejecutar pruebas
test: format mount
	@echo "==> Ejecutando pruebas..."
	./test_fs.sh $(MNTDIR)
	@echo "==> Pruebas completadas."
	@fusermount -u $(MNTDIR)

prints:
	@echo "==> Montando el sistema de ficheros..."
	@mkdir -p $(MNTDIR)
	@echo "==> Montando $(IMG) en $(MNTDIR)..."
	./$(EXEC) $(IMG) $(MNTDIR) -f

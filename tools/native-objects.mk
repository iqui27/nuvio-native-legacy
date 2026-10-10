# GNU make, nas imagens SDK existentes. Objetos e dependencias sobrevivem ao container.
# CACHE_KEY inclui flags/configuracao e a identidade da imagem; -MD inclui headers externos.
.DELETE_ON_ERROR:
.DEFAULT_GOAL := all
# Medido: qjs.c terminava sozinho ~50s apos os demais TUs no TPK emulado.
# Apenas a fila de compilacao muda; os scripts preservam a ordem de link.
ORDERED := $(filter src/qjs.c,$(SOURCES)) $(filter-out src/qjs.c,$(SOURCES))
OBJECTS := $(addprefix $(OBJDIR)/,$(ORDERED:.c=.o))
.PHONY: all
all: $(OBJECTS)
$(OBJDIR)/%.o: %.c tools/native-objects.mk
	@mkdir -p $(dir $@)
	@echo "CC $<"
	@$(CC) $(NATIVE_FLAGS) $(if $(filter src/p2pmotor_motor.c,$<),$(MOTOR_FLAGS)) $(if $(filter src/p2pmotor.c,$<),-D_FILE_OFFSET_BITS=64) -MD -MP -MF $(@:.o=.d) -c $< -o $@
-include $(OBJECTS:.o=.d)

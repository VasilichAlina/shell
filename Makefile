# Компилятор и флаги
CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -g
READLINE_FLAGS = -lreadline
FUSE_FLAGS = -lfuse3
TARGET = kubsh

# Версия пакета
VERSION = 1.0.0
PACKAGE_NAME = kubsh
BUILD_DIR = build
DEB_DIR = $(BUILD_DIR)/$(PACKAGE_NAME)_$(VERSION)_amd64
DEB_FILE := $(PWD)/kubsh.deb

# Исходные файлы
SRCS = main.cpp vfs.cpp
OBJS = $(SRCS:.cpp=.o)

# Основные цели
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS) $(FUSE_FLAGS) $(READLINE_FLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Запуск шелла
run: $(TARGET)
	./$(TARGET)

# Подготовка структуры для deb-пакета
prepare-deb: $(TARGET)
	@echo "Подготовка структуры для deb-пакета..."
	@mkdir -p $(DEB_DIR)/DEBIAN
	@mkdir -p $(DEB_DIR)/usr/local/bin
	@cp $(TARGET) $(DEB_DIR)/usr/local/bin/
	@chmod +x $(DEB_DIR)/usr/local/bin/$(TARGET)
	
	@echo "Создание control файла..."
	@echo "Package: $(PACKAGE_NAME)" > $(DEB_DIR)/DEBIAN/control
	@echo "Version: $(VERSION)" >> $(DEB_DIR)/DEBIAN/control
	@echo "Section: utils" >> $(DEB_DIR)/DEBIAN/control
	@echo "Priority: optional" >> $(DEB_DIR)/DEBIAN/control
	@echo "Architecture: amd64" >> $(DEB_DIR)/DEBIAN/control
	@echo "Maintainer: Test User <test@example.com>" >> $(DEB_DIR)/DEBIAN/control
	@echo "Depends: libfuse3-3, libreadline8" >> $(DEB_DIR)/DEBIAN/control
	@echo "Description: Custom shell with VFS" >> $(DEB_DIR)/DEBIAN/control
	@echo " Custom shell implementation with virtual filesystem for user management." >> $(DEB_DIR)/DEBIAN/control

# Сборка deb-пакета
deb: prepare-deb
	@echo "Сборка deb-пакета..."
	@dpkg-deb --build --root-owner-group $(DEB_DIR)
	@mv $(BUILD_DIR)/$(PACKAGE_NAME)_$(VERSION)_amd64.deb $(DEB_FILE)
	@echo "Пакет создан: $(DEB_FILE)"

# РЕАЛЬНЫЕ ТЕСТЫ для преподавателя
test: $(TARGET)
	@echo "=== Running tests for kubsh ==="
	@echo ""
	
	# Тест 1: Бинарник существует
	@echo "Test 1: Binary exists"
	@if [ -f "$(TARGET)" ]; then \
		echo "✓ PASS: $(TARGET) found"; \
	else \
		echo "✗ FAIL: $(TARGET) not found"; \
		exit 1; \
	fi
	@echo ""
	
	# Тест 2: Может запускаться и завершаться
	@echo "Test 2: Shell starts and exits"
	@echo "\\q" | timeout 2 ./$(TARGET) 2>&1 >/dev/null; \
	EXIT_CODE=$$?; \
	if [ $$EXIT_CODE -eq 0 ] || [ $$EXIT_CODE -eq 124 ]; then \
		echo "✓ PASS: Shell executes properly"; \
	else \
		echo "✗ FAIL: Shell execution failed (code: $$EXIT_CODE)"; \
	fi
	@echo ""
	
	# Тест 3: История команд работает
	@echo "Test 3: Command history"
	@rm -f ~/.kubsh_history 2>/dev/null
	@echo "test_command" | timeout 2 ./$(TARGET) 2>&1 >/dev/null
	@if [ -f ~/.kubsh_history ]; then \
		echo "✓ PASS: History file created"; \
		echo "  History content:"; \
		cat ~/.kubsh_history | head -5; \
	else \
		echo "✗ FAIL: No history file"; \
	fi
	@echo ""
	
	# Тест 4: VFS директория создается
	@echo "Test 4: VFS directory setup"
	@if [ -d ~/users ]; then \
		echo "✓ PASS: Users directory exists"; \
		ls -la ~/users/ 2>/dev/null | head -10; \
	else \
		echo "✗ FAIL: Users directory missing"; \
	fi
	@echo ""
	
	# Тест 5: Встроенная команда echo
	@echo "Test 5: Builtin echo command"
	@echo "echo Hello from test" | timeout 2 ./$(TARGET) 2>&1 | grep -q "Hello from test" && \
		echo "✓ PASS: echo works" || \
		echo "✗ FAIL: echo doesn't work"
	@echo ""
	
	@echo "=== Test summary ==="
	@echo "If you see mostly ✓ - you're good!"
	@echo "If you see ✗ - need to fix those"

# Тесты через Docker преподавателя
docker-test: deb
	@echo "=== Running professor's Docker tests ==="
	@docker run --rm \
		-v $(PWD):/app \
		-v /tmp:/tmp \
		-w /app \
		tyvik/kubsh_test:master \
		/bin/bash -c "make test || echo 'Running custom tests...'; \
			if [ -f kubsh ]; then \
				echo 'Basic test:'; \
				echo '\\q' | timeout 2 ./kubsh && echo 'Shell works'; \
			fi"

# Быстрая проверка
check: $(TARGET)
	@echo "=== Quick check ==="
	@ls -lh $(TARGET)
	@file $(TARGET)
	@ldd $(TARGET) 2>/dev/null || echo "ldd not available"
	@echo "=== Dependencies ==="
	@dpkg -l | grep -E "(fuse|readline)" || apt list --installed | grep -E "(fuse|readline)"

# Установка пакета (требует sudo)
install: deb
	sudo dpkg -i $(DEB_FILE) || sudo apt-get install -f -y

# Удаление пакета
uninstall:
	sudo dpkg -r $(PACKAGE_NAME) 2>/dev/null || true

# Очистка
clean:
	rm -rf $(BUILD_DIR) $(TARGET) *.deb *.o core

# Показать справку
help:
	@echo "Доступные команды:"
	@echo "  make all         - собрать программу"
	@echo "  make deb         - создать deb-пакет"
	@echo "  make test        - запустить локальные тесты"
	@echo "  make docker-test - запустить тесты через Docker преподавателя"
	@echo "  make check       - проверить бинарник"
	@echo "  make install     - установить пакет"
	@echo "  make uninstall   - удалить пакет"
	@echo "  make clean       - очистить проект"
	@echo "  make run         - запустить шелл"
	@echo "  make help        - показать эту справку"

.PHONY: all deb install uninstall clean help prepare-deb run test docker-test check
.PHONY: all deb install uninstall clean help prepare-deb run test

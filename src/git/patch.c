        const char *line = hunk->lines.data[i];

            if (!reverse && line[0] == '+') {
            } else if (reverse && line[0] == '-') {
        size_t len = strlen(line);
        memcpy(ptr, line, len);
            if (!reverse && line[0] == '-') {
            } else if (reverse && line[0] == '+') {
        } else if (line[0] == '-' || line[0] == '+') has_changes = true;

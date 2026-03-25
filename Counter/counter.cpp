#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <sys/stat.h>
#include <algorithm>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <omp.h>
#include <fcntl.h>
#include <unistd.h>
#include <unordered_set>

using namespace std;

inline bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

inline char to_lower(char c) {
    return (c >= 'A' && c <= 'Z') ? c + 32 : c;
}

int main() {
    auto t_start = chrono::steady_clock::now();

    cout << "========================================" << endl;
    cout << "Iniciando programa..." << endl;

    const char* filepath = "wikipedia.txt";

    struct stat sb;
    if (stat(filepath, &sb) != 0) {
        cerr << "Error al obtener tamaño del archivo. Revisa la ruta." << endl;
        return 1;
    }
    size_t filesize = static_cast<size_t>(sb.st_size);

    int fd = open(filepath, O_RDONLY);
    if (fd < 0) {
        cerr << "Error al abrir el archivo. Revisa la ruta/permisos." << endl;
        return 1;
    }

    unordered_map<string, long long> global_counts;
    global_counts.reserve(300000);

    const size_t BLOCK_SIZE = 4 * 1024 * 1024; // 4 MB
    const size_t num_blocks = (filesize + BLOCK_SIZE - 1) / BLOCK_SIZE;

    size_t total_processed = 0;
    int last_percent = -1;

    cout << "Leyendo por bloques en paralelo con OpenMP..." << endl;

    const unordered_set<string> stopwords = {
    "the","and","of","to","in","a","is","that","it","was",
    "he","for","with","as","on","by","at","from","his",
    "be","this","which","or","are","an","not","all",

    /*
    "but","if","while","can","will","just","so","than","too","very",
    "about","into","over","after","before","between","out","up","down",
    "off","again","further","then","once",

    "here","there","when","where","why","how",

    "any","both","each","few","more","most","other","some","such",
    "no","nor","only","own","same","such","too","very",

    "s","t","don","should","now",

    "d","ll","m","o","re","ve","y",

    "ain","aren","couldn","didn","doesn","hadn","hasn","haven",
    "isn","ma","mightn","mustn","needn","shan","shouldn","wasn","weren","won","wouldn",

    "i","me","my","myself","we","our","ours","ourselves",
    "you","your","yours","yourself","yourselves",
    "him","himself","she","her","hers","herself",
    "its","itself","they","them","their","theirs","themselves"*/
};

    #pragma omp parallel for schedule(dynamic)
    for (size_t block = 0; block < num_blocks; ++block) {
        size_t start = block * BLOCK_SIZE;
        size_t bytes_to_read = min(BLOCK_SIZE, filesize - start);

        vector<char> buffer(bytes_to_read);
        ssize_t got = pread(fd, buffer.data(), bytes_to_read, static_cast<off_t>(start));
        if (got <= 0) continue;

        size_t bytes_read = static_cast<size_t>(got);
        size_t i = 0;
        size_t end = bytes_read;

        // Ajuste de inicio: si el bloque empieza en mitad de palabra, saltar hasta delimitador
        if (start > 0 && bytes_read > 0) {
            char prev_char = 0;
            if (pread(fd, &prev_char, 1, static_cast<off_t>(start - 1)) == 1) {
                if (is_alpha(prev_char) && is_alpha(buffer[0])) {
                    while (i < end && is_alpha(buffer[i])) ++i;
                }
            }
        }

        // Ajuste de fin: si el bloque termina en mitad de palabra, recortar cola parcial
        if ((start + bytes_read) < filesize && end > i) {
            char next_char = 0;
            if (pread(fd, &next_char, 1, static_cast<off_t>(start + bytes_read)) == 1) {
                if (is_alpha(buffer[end - 1]) && is_alpha(next_char)) {
                    while (end > i && is_alpha(buffer[end - 1])) --end;
                }
            }
        }

        unordered_map<string, long long> local_counts;
        local_counts.reserve(4096);

        string current_word;
        current_word.reserve(64);

        for (size_t j = i; j < end; ++j) {
            char c = buffer[j];
            if (is_alpha(c)) {
                current_word += to_lower(c);
            } else if (!current_word.empty()) {
                if (stopwords.find(current_word) == stopwords.end()) {
                    local_counts[current_word]++;
                }
                current_word.clear();
            }
        }
        if (!current_word.empty()) {
            if (stopwords.find(current_word) == stopwords.end()) {
                local_counts[current_word]++;
            }
        }

        #pragma omp critical(merge_counts)
        {
            for (const auto& kv : local_counts) {
                global_counts[kv.first] += kv.second;
            }
        }

        size_t done_now;
        #pragma omp atomic capture
        {
            total_processed += bytes_read;
            done_now = total_processed;
        }
        int percent = (filesize > 0) ? static_cast<int>((done_now * 100) / filesize) : 100;

        #pragma omp critical(progress_print)
        {
            if (percent > last_percent) {
                last_percent = percent;
                cout << "\rProgreso lectura: " << setw(3) << percent
                     << "% (" << done_now << "/" << filesize << " bytes)" << flush;
            }
        }
    }

    close(fd);

    cout << "\rProgreso lectura: 100% (" << filesize << "/" << filesize << " bytes)" << endl;
    cout << "Ordenando resultados..." << endl;

    vector<pair<string, long long>> sorted_counts(global_counts.begin(), global_counts.end());
    sort(sorted_counts.begin(), sorted_counts.end(),
         [](const auto& a, const auto& b) { return a.second > b.second; });

    ofstream outfile("frecuencia_palabras.txt");
    if (outfile.is_open()) {
        for (const auto& p : sorted_counts) {
            outfile << p.first << " " << p.second << "\n";
        }
        outfile.close();
    } else {
        cerr << "Error al crear el archivo de salida." << endl;
        return 1;
    }

    auto t_end = chrono::steady_clock::now();
    chrono::duration<double> elapsed = t_end - t_start;

    cout << "========================================" << endl;
    cout << "Total de palabras únicas: " << global_counts.size() << endl;
    cout << "Resultados guardados en 'frecuencia_palabras.txt'" << endl;
    cout << "Tiempo total de ejecución: " << elapsed.count() << " segundos." << endl;
    cout << "========================================" << endl;

    return 0;
}
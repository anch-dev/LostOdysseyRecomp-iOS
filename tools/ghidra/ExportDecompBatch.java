// Decompile only existing functions named by an address file; never modify the program.
// Arguments: new output directory, address file (one hex address per line), optional timeout seconds.
// Run with -process default.xex -noanalysis -readOnly.
// @category LostOdyssey
import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import com.google.gson.JsonObject;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.util.exception.CancelledException;
import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.IOException;
import java.io.InputStream;
import java.io.UncheckedIOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.security.MessageDigest;
import java.util.Comparator;
import java.util.HexFormat;
import java.util.LinkedHashMap;
import java.util.Locale;
import java.util.Map;
import java.util.stream.Stream;

public class ExportDecompBatch extends GhidraScript {
    private static final Gson JSON = new GsonBuilder().serializeNulls().create();

    private long indexedFunctionCount() throws Exception {
        long count = 0;
        FunctionIterator iterator = currentProgram.getFunctionManager().getFunctions(true);
        while (iterator.hasNext()) {
            monitor.checkCancelled();
            if (!iterator.next().isExternal()) count++;
        }
        return count;
    }

    private static String sha256(Path path) throws Exception {
        MessageDigest digest = MessageDigest.getInstance("SHA-256");
        try (InputStream input = Files.newInputStream(path)) {
            byte[] buffer = new byte[65536];
            int length;
            while ((length = input.read(buffer)) != -1) digest.update(buffer, 0, length);
        }
        return HexFormat.of().formatHex(digest.digest());
    }

    private static String address(Address value) {
        return String.format(Locale.ROOT, "%08X", value.getOffset());
    }

    private JsonObject programRecord() throws Exception {
        String sha = currentProgram.getExecutableSHA256();
        if (sha == null || !sha.matches("(?i)[0-9a-f]{64}")) {
            throw new IllegalStateException("Program has no valid executable SHA-256; cannot identify the imported XEX");
        }
        JsonObject row = new JsonObject();
        row.addProperty("kind", "program");
        row.addProperty("name", currentProgram.getName());
        row.addProperty("executable_path", currentProgram.getExecutablePath());
        row.addProperty("executable_sha256", sha.toLowerCase(Locale.ROOT));
        row.addProperty("image_base", address(currentProgram.getImageBase()));
        row.addProperty("language", currentProgram.getLanguageID().getIdAsString());
        row.addProperty("compiler_spec", currentProgram.getCompilerSpec().getCompilerSpecID().getIdAsString());
        row.addProperty("function_count", indexedFunctionCount());
        return row;
    }

    private static void write(BufferedWriter out, JsonObject row) throws IOException {
        out.write(JSON.toJson(row));
        out.newLine();
    }

    private static void removeTemporaryDirectory(Path directory) throws IOException {
        try (Stream<Path> paths = Files.walk(directory)) {
            try {
                paths.sorted(Comparator.reverseOrder()).forEach(path -> {
                    try {
                        Files.deleteIfExists(path);
                    } catch (IOException e) {
                        throw new UncheckedIOException(e);
                    }
                });
            } catch (UncheckedIOException e) {
                throw e.getCause();
            }
        }
    }

    @Override public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2 || args.length > 3) {
            throw new IllegalArgumentException("Expected output-directory addresses-file [timeout-seconds]");
        }
        int timeout = args.length == 3 ? Integer.parseInt(args[2]) : 45;
        if (timeout < 1) throw new IllegalArgumentException("Timeout must be positive");
        Path output = Path.of(args[0]).toAbsolutePath().normalize();
        Path parent = output.getParent();
        if (!Files.isDirectory(parent)) throw new IllegalArgumentException("Output parent does not exist: " + parent);
        if (Files.exists(output)) throw new IllegalArgumentException("Output directory already exists: " + output);
        Path addressFile = Path.of(args[1]).toAbsolutePath().normalize();
        Map<String, Address> requests = new LinkedHashMap<>();
        try (BufferedReader input = Files.newBufferedReader(addressFile, StandardCharsets.UTF_8)) {
            String line;
            int lineNumber = 0;
            while ((line = input.readLine()) != null) {
                monitor.checkCancelled();
                lineNumber++;
                String value = line.trim();
                if (value.isEmpty()) continue;
                if (!value.matches("(?i)(?:0x)?[0-9a-f]{1,16}")) {
                    throw new IllegalArgumentException("Invalid hex address at line " + lineNumber + ": " + value);
                }
                long offset = Long.parseUnsignedLong(value.replaceFirst("(?i)^0x", ""), 16);
                Address requested = toAddr(offset);
                if (requested.getOffset() != offset) {
                    throw new IllegalArgumentException("Address is outside the program address space at line " + lineNumber);
                }
                requests.putIfAbsent(address(requested), requested);
            }
        }
        if (requests.isEmpty()) throw new IllegalArgumentException("Address file is empty: " + addressFile);

        JsonObject program = programRecord();
        Path temporary = Files.createTempDirectory(parent, output.getFileName() + ".");
        boolean published = false;
        DecompInterface decompiler = new DecompInterface();
        try {
            if (!decompiler.openProgram(currentProgram)) {
                throw new IllegalStateException("Could not open the existing program in the decompiler");
            }
            int ok = 0, missing = 0, failed = 0;
            Map<String, String> cachedC = new LinkedHashMap<>();
            Map<String, String> cachedSha = new LinkedHashMap<>();
            Map<String, String> cachedErrors = new LinkedHashMap<>();
            try (BufferedWriter manifest = Files.newBufferedWriter(
                    temporary.resolve("manifest.jsonl"), StandardCharsets.UTF_8)) {
                write(manifest, program);
                for (Map.Entry<String, Address> request : requests.entrySet()) {
                    monitor.checkCancelled();
                    Function function = currentProgram.getFunctionManager()
                        .getFunctionContaining(request.getValue());
                    if (function == null) {
                        function = currentProgram.getFunctionManager().getFunctionAt(request.getValue());
                    }
                    JsonObject row = new JsonObject();
                    row.addProperty("kind", "decomp");
                    row.addProperty("requested", request.getKey());
                    if (function == null || function.isExternal()) {
                        row.add("address", null);
                        row.add("name", null);
                        row.addProperty("status", "missing");
                        row.addProperty("error", "No existing function contains this address");
                        row.add("filename", null);
                        row.add("sha256", null);
                        missing++;
                    } else {
                        String entry = address(function.getEntryPoint());
                        String filename = entry + ".c";
                        row.addProperty("address", entry);
                        row.addProperty("name", function.getName());
                        if (!cachedC.containsKey(entry) && !cachedErrors.containsKey(entry)) {
                            try {
                                DecompileResults result = decompiler.decompileFunction(function, timeout, monitor);
                                monitor.checkCancelled();
                                if (result != null && result.isCancelled()) throw new CancelledException();
                                if (result != null && result.decompileCompleted() &&
                                        result.getDecompiledFunction() != null) {
                                    String c = result.getDecompiledFunction().getC();
                                    Path cFile = temporary.resolve(filename);
                                    Files.writeString(cFile, c, StandardCharsets.UTF_8);
                                    String digest = sha256(cFile);
                                    cachedC.put(entry, filename);
                                    cachedSha.put(entry, digest);
                                } else {
                                    String error = result == null ? "No decompiler result" : result.getErrorMessage();
                                    if (result != null && result.isTimedOut()) {
                                        error = "Timed out after " + timeout + " seconds" +
                                            (error == null || error.isBlank() ? "" : ": " + error);
                                    }
                                    cachedErrors.put(entry, error == null || error.isBlank()
                                        ? "Decompiler did not complete" : error);
                                }
                            } catch (CancelledException | IOException e) {
                                throw e;
                            } catch (Exception e) {
                                monitor.checkCancelled();
                                cachedErrors.put(entry, e.toString());
                            }
                        }
                        if (cachedC.containsKey(entry)) {
                            row.addProperty("status", "ok");
                            row.add("error", null);
                            row.addProperty("filename", cachedC.get(entry));
                            row.addProperty("sha256", cachedSha.get(entry));
                            ok++;
                        } else {
                            row.addProperty("status", "failed");
                            row.addProperty("error", cachedErrors.get(entry));
                            row.add("filename", null);
                            row.add("sha256", null);
                            failed++;
                        }
                    }
                    write(manifest, row);
                }
                JsonObject complete = new JsonObject();
                complete.addProperty("kind", "complete");
                complete.addProperty("request_count", requests.size());
                complete.addProperty("ok_count", ok);
                complete.addProperty("missing_count", missing);
                complete.addProperty("failed_count", failed);
                write(manifest, complete);
            }
            monitor.checkCancelled();
            if (Files.exists(output)) throw new IllegalArgumentException("Output directory now exists: " + output);
            Files.move(temporary, output, StandardCopyOption.ATOMIC_MOVE);
            published = true;
            println("Exported " + requests.size() + " requests to " + output);
        } finally {
            decompiler.dispose();
            if (!published) removeTemporaryDirectory(temporary);
        }
    }
}

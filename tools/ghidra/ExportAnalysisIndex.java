// Export the existing function inventory and resolved static calls without changing the program.
// Arguments: output JSONL path. Run with -process default.xex -noanalysis -readOnly.
// @category LostOdyssey
import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import com.google.gson.JsonArray;
import com.google.gson.JsonObject;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressRange;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import java.io.BufferedWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.Locale;

public class ExportAnalysisIndex extends GhidraScript {
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

    static String address(Address value) {
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

    static void write(BufferedWriter out, JsonObject row) throws java.io.IOException {
        out.write(JSON.toJson(row));
        out.newLine();
    }

    @Override public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) throw new IllegalArgumentException("Expected output.jsonl");
        Path output = Path.of(args[0]).toAbsolutePath().normalize();
        Path parent = output.getParent();
        if (!Files.isDirectory(parent)) throw new IllegalArgumentException("Output parent does not exist: " + parent);
        JsonObject program = programRecord();
        Path temporary = Files.createTempFile(parent, output.getFileName() + ".", ".tmp");
        boolean published = false;
        try {
            long functions = 0;
            long calls = 0;
            try (BufferedWriter out = Files.newBufferedWriter(temporary, StandardCharsets.UTF_8)) {
                write(out, program);
                FunctionIterator iterator = currentProgram.getFunctionManager().getFunctions(true);
                while (iterator.hasNext()) {
                    monitor.checkCancelled();
                    Function function = iterator.next();
                    if (function.isExternal()) continue;
                    JsonObject row = new JsonObject();
                    row.addProperty("kind", "function");
                    row.addProperty("address", address(function.getEntryPoint()));
                    row.addProperty("name", function.getName());
                    row.addProperty("signature", function.getPrototypeString(false, false));
                    JsonArray ranges = new JsonArray();
                    for (AddressRange range : function.getBody()) {
                        JsonArray pair = new JsonArray();
                        pair.add(address(range.getMinAddress()));
                        pair.add(address(range.getMaxAddress()));
                        ranges.add(pair);
                    }
                    row.add("body_ranges", ranges);
                    row.addProperty("is_thunk", function.isThunk());
                    row.addProperty("is_external", function.isExternal());
                    write(out, row);
                    functions++;
                }

                ReferenceIterator references = currentProgram.getReferenceManager()
                    .getReferenceIterator(currentProgram.getMinAddress());
                while (references.hasNext()) {
                    monitor.checkCancelled();
                    Reference reference = references.next();
                    if (!reference.getReferenceType().isCall()) continue;
                    Function caller = currentProgram.getFunctionManager()
                        .getFunctionContaining(reference.getFromAddress());
                    Function callee = currentProgram.getFunctionManager()
                        .getFunctionAt(reference.getToAddress());
                    if (caller == null || callee == null || caller.isExternal() || callee.isExternal()) continue;
                    JsonObject row = new JsonObject();
                    row.addProperty("kind", "call");
                    row.addProperty("caller", address(caller.getEntryPoint()));
                    row.addProperty("callee", address(callee.getEntryPoint()));
                    row.addProperty("site", address(reference.getFromAddress()));
                    row.addProperty("reference_type", reference.getReferenceType().toString());
                    row.addProperty("computed", reference.getReferenceType().isComputed());
                    write(out, row);
                    calls++;
                }
                JsonObject complete = new JsonObject();
                complete.addProperty("kind", "complete");
                complete.addProperty("function_count", functions);
                complete.addProperty("call_count", calls);
                write(out, complete);
            }
            monitor.checkCancelled();
            Files.move(temporary, output, StandardCopyOption.ATOMIC_MOVE,
                StandardCopyOption.REPLACE_EXISTING);
            published = true;
            println("Exported " + functions + " functions and " + calls + " calls to " + output);
        } finally {
            if (!published) Files.deleteIfExists(temporary);
        }
    }
}

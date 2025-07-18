// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package ai.vespa.embedding;

import ai.vespa.modelintegration.evaluator.OnnxEvaluator;
import ai.vespa.modelintegration.evaluator.OnnxEvaluatorOptions;
import ai.vespa.modelintegration.utils.OnnxExternalDataResolver;
import com.yahoo.api.annotations.Beta;
import ai.vespa.modelintegration.evaluator.OnnxRuntime;
import com.yahoo.component.AbstractComponent;
import com.yahoo.component.annotation.Inject;
import com.yahoo.embedding.ColBertEmbedderConfig;
import com.yahoo.language.huggingface.HuggingFaceTokenizer;
import com.yahoo.language.process.Embedder;
import com.yahoo.searchlib.rankingexpression.evaluation.MapContext;
import com.yahoo.searchlib.rankingexpression.evaluation.TensorValue;
import com.yahoo.searchlib.rankingexpression.rule.ReferenceNode;
import com.yahoo.searchlib.rankingexpression.rule.UnpackBitsNode;
import com.yahoo.tensor.IndexedTensor;
import com.yahoo.tensor.Tensor;
import com.yahoo.tensor.TensorAddress;
import com.yahoo.tensor.TensorType;

import java.nio.file.Paths;
import java.util.Map;
import java.util.List;
import java.util.ArrayList;
import java.util.Set;
import java.util.HashSet;
import java.util.BitSet;

import static com.yahoo.language.huggingface.ModelInfo.TruncationStrategy.LONGEST_FIRST;

/**
 * A ColBERT embedder implementation that maps text to multiple vectors, one vector per token subword id.
 * This embedder uses a HuggingFace tokenizer to produce a token sequence that is then input to a transformer model.
 *
 * See col-bert-embedder.def for configurable parameters.
 *
 * @author bergum
 */
@Beta
public class ColBertEmbedder extends AbstractComponent implements Embedder {

    private static final String PUNCTUATION = "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";

    private final Embedder.Runtime runtime;
    private final String inputIdsName;
    private final String attentionMaskName;
    private final String outputName;
    private final HuggingFaceTokenizer tokenizer;
    private final OnnxEvaluator evaluator;
    private final int maxTransformerTokens;
    private final int maxQueryTokens;
    private final int maxDocumentTokens;

    private final long startSequenceToken;
    private final long endSequenceToken;
    private final long maskSequenceToken;

    private final long padSequenceToken;

    private final long querySequenceToken;

    private final long documentSequenceToken;
    private final Set<Long> skipTokens;

    public record TransformerInput(List<Long> inputIds, List<Long> attentionMask) {}

    @Inject
    public ColBertEmbedder(OnnxRuntime onnx, Embedder.Runtime runtime, ColBertEmbedderConfig config) {
        this.runtime = runtime;
        inputIdsName = config.transformerInputIds();
        attentionMaskName = config.transformerAttentionMask();
        outputName = config.transformerOutput();
        maxTransformerTokens = config.transformerMaxTokens();
        maxDocumentTokens = Math.min(config.maxDocumentTokens(), maxTransformerTokens);
        maxQueryTokens = Math.min(config.maxQueryTokens(), maxTransformerTokens);
        startSequenceToken = config.transformerStartSequenceToken();
        endSequenceToken = config.transformerEndSequenceToken();
        maskSequenceToken = config.transformerMaskToken();
        padSequenceToken = config.transformerPadToken();
        querySequenceToken = config.queryTokenId();
        documentSequenceToken = config.documentTokenId();

        var tokenizerPath = Paths.get(config.tokenizerPath().toString());
        var builder = new HuggingFaceTokenizer.Builder()
                .addSpecialTokens(false)
                .addDefaultModel(tokenizerPath)
                .setPadding(false);
        var info = HuggingFaceTokenizer.getModelInfo(tokenizerPath);
        if (info.maxLength() == -1 || info.truncation() != LONGEST_FIRST) {
            // Force truncation
            // to max length accepted by model if tokenizer.json contains no valid truncation configuration
            int maxLength = info.maxLength() > 0 && info.maxLength() <= config.transformerMaxTokens()
                    ? info.maxLength()
                    : config.transformerMaxTokens();
            builder.setTruncation(true).setMaxLength(maxLength);
        }
        this.tokenizer = builder.build();
        this.skipTokens = new HashSet<>();
        PUNCTUATION.chars().forEach(
                c -> this.skipTokens.addAll(
                        tokenizer.encode(Character.toString((char) c), null).ids())
        );
        var optionsBuilder = new OnnxEvaluatorOptions.Builder()
                .setExecutionMode(config.transformerExecutionMode().toString())
                .setThreads(config.transformerInterOpThreads(), config.transformerIntraOpThreads());
        if (config.transformerGpuDevice() >= 0)
            optionsBuilder.setGpuDevice(config.transformerGpuDevice());
        var onnxOpts = optionsBuilder.build();
        var resolver = new OnnxExternalDataResolver();
        evaluator = onnx.evaluatorOf(resolver.resolveOnnxModel(config.transformerModelReference()).toString(), onnxOpts);

        validateModel();
    }

    private void validateModel() {
        Map<String, TensorType> inputs = evaluator.getInputInfo();
        validateName(inputs, inputIdsName, "input");
        validateName(inputs, attentionMaskName, "input");
        Map<String, TensorType> outputs = evaluator.getOutputInfo();
        validateName(outputs, outputName, "output");
    }

    private static void validateName(Map<String, TensorType> types, String name, String type) {
        if (!types.containsKey(name)) {
            throw new IllegalArgumentException("Model does not contain required " + type + ": '" + name + "'. " +
                                               "Model contains: " + String.join(",", types.keySet()));
        }
    }

    @Override
    public List<Integer> embed(String text, Context context) {
        throw new UnsupportedOperationException("This embedder only supports embed with tensor type");
    }

    @Override
    public Tensor embed(String text, Context context, TensorType tensorType) {
        if ( ! validTensorType(tensorType)) {
            throw new IllegalArgumentException("Invalid colbert embedder tensor target destination. " +
                                               "Wanted a mixed 2-d mapped-indexed tensor, got " + tensorType);
        }
        if (context.getDestination().startsWith("query")) {
            return embedQuery(text, context, tensorType);
        } else {
            return embedDocument(text, context, tensorType);
        }
    }
    @Override
    public void deconstruct() {
        evaluator.close();
        tokenizer.close();
    }

    protected TransformerInput buildTransformerInput(List<Long> tokens, int maxTokens, boolean isQuery) {
        if (!isQuery) {
            tokens = tokens.stream().filter(token -> !skipTokens.contains(token)).toList();
        }
        List<Long> inputIds = new ArrayList<>(maxTokens);
        List<Long> attentionMask = new ArrayList<>(maxTokens);
        if (tokens.size() > maxTokens - 3)
            tokens = tokens.subList(0, maxTokens - 3);
        inputIds.add(startSequenceToken);
        inputIds.add(isQuery? querySequenceToken: documentSequenceToken);
        inputIds.addAll(tokens);
        inputIds.add(endSequenceToken);

        int inputLength = inputIds.size();
        long padTokenId = isQuery? maskSequenceToken: padSequenceToken;

        int padding = isQuery? maxTokens - inputLength: 0;
        for (int i = 0; i < padding; i++)
            inputIds.add(padTokenId);

        for (int i = 0; i < inputLength; i++)
            attentionMask.add((long) 1);

        for (int i = 0; i < padding; i++)
            attentionMask.add((long) 0); // Do not attend to mask paddings

        return new TransformerInput(inputIds, attentionMask);
    }

    protected Tensor embedQuery(String text, Context context, TensorType tensorType) {
        if (tensorType.valueType() == TensorType.Value.INT8)
            throw new IllegalArgumentException("ColBert query embed does not accept int8 tensor value type");

        EmbeddingResult result = lookupOrEvaluate(context, text, true);
        return toFloatTensor((IndexedTensor)result.outputs.get(outputName), tensorType, result.inputIdSize);
    }

    protected Tensor embedDocument(String text, Context context, TensorType tensorType) {
        EmbeddingResult result = lookupOrEvaluate(context, text, false);
        var modelOutput = (IndexedTensor)result.outputs.get(outputName);
        if (tensorType.valueType() == TensorType.Value.INT8)
            return toBitTensor(modelOutput, tensorType, result.inputIdSize);
        else
            return toFloatTensor(modelOutput, tensorType, result.inputIdSize);
    }

    /**
     * Evaluate the embedding model if the result is not present in the context cache.
     *
     * @param context the context accompanying the request
     * @param text the text that is embedded
     * @return the model output
     */
    protected EmbeddingResult lookupOrEvaluate(Context context, String text, boolean isQuery) {
        var key = new EmbedderCacheKey(context.getEmbedderId(), text);
        return context.computeCachedValueIfAbsent(key, () -> evaluate(context, text, isQuery));
    }

    private EmbeddingResult evaluate(Context context, String text, boolean isQuery) {
        var start = System.nanoTime();
        var encoding = tokenizer.encode(text, context.getLanguage());
        runtime.sampleSequenceLength(encoding.ids().size(), context);
        TransformerInput input = buildTransformerInput(encoding.ids(), isQuery ? maxQueryTokens : maxDocumentTokens, isQuery);
        Tensor inputIdsTensor = createTensorRepresentation(input.inputIds, "d1");
        Tensor attentionMaskTensor = createTensorRepresentation(input.attentionMask, "d1");
        var inputs = Map.of(inputIdsName,
                            inputIdsTensor.expand("d0"),
                            attentionMaskName, attentionMaskTensor.expand("d0"));
        Map<String, Tensor> outputs = evaluator.evaluate(inputs);
        runtime.sampleEmbeddingLatency((System.nanoTime() - start) / 1_000_000d, context);
        return new EmbeddingResult(input.inputIds.size(), outputs);
    }

    public static Tensor toFloatTensor(IndexedTensor result, TensorType type, int nTokens) {
        if (result.shape().length != 3)
            throw new IllegalArgumentException("Expected onnx result to have 3-dimensions [batch, sequence, dim]");
        int size = type.indexedSubtype().dimensions().size();
        if (size != 1)
            throw new IllegalArgumentException("Target indexed sub-type must have one dimension");
        int wantedDimensionality = type.indexedSubtype().dimensions().get(0).size().get().intValue();
        int resultDimensionality = (int)result.shape()[2];
        if (wantedDimensionality > resultDimensionality) {
            throw new IllegalArgumentException("Not possible to map token vector embedding with " + resultDimensionality +
                                               " dimensions into tensor with " + wantedDimensionality);
        }
        Tensor.Builder builder = Tensor.Builder.of(type);
        for (int token = 0; token < nTokens; token++) {
            for (int d = 0; d < wantedDimensionality; d++) {
                var value = result.get(0,token,d); // batch, sequence token, dimension
                builder.cell(TensorAddress.of(token,d),value);
            }
        }
        return builder.build();
    }

    public static Tensor toBitTensor(IndexedTensor result, TensorType type, int nTokens) {
        if (type.valueType() != TensorType.Value.INT8)
            throw new IllegalArgumentException("Only a int8 tensor type can be the destination of bit packing");
        if(result.shape().length != 3)
            throw new IllegalArgumentException("Expected onnx result to have 3-dimensions [batch, sequence, dim]");

        int size = type.indexedSubtype().dimensions().size();
        if (size != 1)
            throw new IllegalArgumentException("Target indexed sub-type must have one dimension");
        int wantedDimensionality = type.indexedSubtype().dimensions().get(0).size().get().intValue();
        //Allow using the first n float dimensions to pack into int8
        int floatDimensionality = 8 * wantedDimensionality;
        int resultDimensionality = (int)result.shape()[2];
        if (floatDimensionality > resultDimensionality) {
            throw new IllegalArgumentException("Not possible to pack " + resultDimensionality +
                                               " + dimensions into " + wantedDimensionality + " dimensions");
        }
        Tensor.Builder builder = Tensor.Builder.of(type);
        for (int token = 0; token < nTokens; token++) {
            BitSet bitSet = new BitSet(8);
            int key = 0;
            for (int d = 0; d < floatDimensionality; d++) {
                var value = result.get(0, token, d); // batch, sequence token, dimension
                int bitIndex = 7 - (d % 8);
                if (value > 0.0) {
                    bitSet.set(bitIndex);
                } else {
                    bitSet.clear(bitIndex);
                }
                if ((d + 1) % 8 == 0) {
                    byte[] bytes = bitSet.toByteArray();
                    byte packed = (bytes.length == 0) ? 0 : bytes[0];
                    builder.cell(TensorAddress.of(token, key), packed);
                    key++;
                    bitSet = new BitSet(8);
                }
            }
        }
        return builder.build();
    }

    public Set<Long> getSkipTokens() {
        return this.skipTokens;
    }

    public static Tensor expandBitTensor(Tensor packed) {
        var unpacker = new UnpackBitsNode(new ReferenceNode("input"), TensorType.Value.FLOAT, "big");
        var context = new MapContext();
        context.put("input", new TensorValue(packed));
        return unpacker.evaluate(context).asTensor();
    }

    protected boolean validTensorType(TensorType target) {
        return target.dimensions().size() == 2 && target.indexedSubtype().rank() == 1;
    }

    private IndexedTensor createTensorRepresentation(List<Long> input, String dimension) {
        int size = input.size();
        TensorType type = new TensorType.Builder(TensorType.Value.FLOAT).indexed(dimension, size).build();
        IndexedTensor.Builder builder = IndexedTensor.Builder.of(type);
        for (int i = 0; i < size; ++i) {
            builder.cell(input.get(i), i);
        }
        return builder.build();
    }

    record EmbedderCacheKey(String embedderId, Object embeddedValue) { }

    record EmbeddingResult(int inputIdSize, Map<String, Tensor> outputs) { }

}

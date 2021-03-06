// Copyright 2017 Yahoo Holdings. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.vespa.config.protocol;

import com.yahoo.text.AbstractUtf8Array;
import com.yahoo.vespa.config.ConfigPayload;

import java.io.IOException;
import java.io.OutputStream;
import java.nio.ByteBuffer;

/**
 * Class for serializing config responses based on {@link com.yahoo.slime.Slime} implementing the {@link ConfigResponse} interface.
 *
 * @author Ulf Lilleengen
 */
public class SlimeConfigResponse implements ConfigResponse {

    private final AbstractUtf8Array payload;
    private final CompressionInfo compressionInfo;
    private final long generation;
    private final boolean applyOnRestart;
    private final String configMd5;

    public static SlimeConfigResponse fromConfigPayload(ConfigPayload payload, long generation,
                                                        boolean applyOnRestart, String configMd5) {
        AbstractUtf8Array data = payload.toUtf8Array(true);
        return new SlimeConfigResponse(data, generation, applyOnRestart,
                                       configMd5,
                                       CompressionInfo.create(CompressionType.UNCOMPRESSED, data.getByteLength()));
    }

    public SlimeConfigResponse(AbstractUtf8Array payload,
                               long generation,
                               boolean applyOnRestart,
                               String configMd5,
                               CompressionInfo compressionInfo) {
        this.payload = payload;
        this.generation = generation;
        this.applyOnRestart = applyOnRestart;
        this.configMd5 = configMd5;
        this.compressionInfo = compressionInfo;
    }

    @Override
    public AbstractUtf8Array getPayload() {
        return payload;
    }

    @Override
    public long getGeneration() {
        return generation;
    }

    @Override
    public boolean applyOnRestart() { return applyOnRestart; }

    @Override
    public String getConfigMd5() {
        return configMd5;
    }

    @Override
    public void serialize(OutputStream os, CompressionType type) throws IOException {
        ByteBuffer buf = Payload.from(payload, compressionInfo).withCompression(type).getData().wrap();
        os.write(buf.array(), buf.arrayOffset()+buf.position(), buf.remaining());
    }

    @Override
    public String toString() {
        return "generation=" + generation +  "\n" +
                "configmd5=" + configMd5 +  "\n" +
                Payload.from(payload, compressionInfo).withCompression(CompressionType.UNCOMPRESSED);
    }

    @Override
    public CompressionInfo getCompressionInfo() { return compressionInfo; }

}

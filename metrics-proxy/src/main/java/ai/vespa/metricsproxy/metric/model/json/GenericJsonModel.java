// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package ai.vespa.metricsproxy.metric.model.json;

import com.fasterxml.jackson.annotation.JsonIgnoreProperties;
import com.fasterxml.jackson.annotation.JsonInclude;
import com.fasterxml.jackson.annotation.JsonProperty;
import com.fasterxml.jackson.annotation.JsonPropertyOrder;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.util.List;

import static com.fasterxml.jackson.annotation.JsonInclude.Include.NON_ABSENT;
import static java.nio.charset.StandardCharsets.UTF_8;

/**
 * @author gjoranv
 */
@JsonIgnoreProperties(ignoreUnknown = true)
@JsonInclude(NON_ABSENT)
@JsonPropertyOrder({ "hostname", "role", "node", "services" })
public class GenericJsonModel {

    @JsonProperty("hostname")
    public String hostname;

    @JsonProperty("role")
    public String role;

    @JsonProperty("node")
    public GenericNode node;

    @JsonProperty("services")
    public List<GenericService> services;

    public String serialize() {
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        serialize(out);
        return out.toString(UTF_8);
    }

    public void serialize(OutputStream out) {
        try {
            JacksonUtil.objectMapper().writeValue(out, this);
        } catch (IOException e) {
            throw new JsonRenderingException("Could not render metrics. Check the log for details.", e);
        }
    }

}

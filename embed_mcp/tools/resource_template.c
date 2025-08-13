#include "resource_interface.h"
#include "utils/logging.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// =============================================================================
// Resource Template Implementation
// =============================================================================

mcp_resource_template_t *mcp_resource_template_create(const char *uri_template,
                                                      const char *name,
                                                      const char *title,
                                                      const char *description,
                                                      const char *mime_type) {
    if (!uri_template || !name) {
        return NULL;
    }

    mcp_resource_template_t *template = calloc(1, sizeof(mcp_resource_template_t));
    if (!template) {
        return NULL;
    }

    template->uri_template = strdup(uri_template);
    template->name = strdup(name);
    template->title = title ? strdup(title) : NULL;
    template->description = description ? strdup(description) : NULL;
    template->mime_type = mime_type ? strdup(mime_type) : NULL;

    if (!template->uri_template || !template->name) {
        mcp_resource_template_destroy(template);
        return NULL;
    }

    return template;
}

void mcp_resource_template_destroy(mcp_resource_template_t *template) {
    if (!template) return;

    free(template->uri_template);
    free(template->name);
    free(template->title);
    free(template->description);
    free(template->mime_type);

    // Free parameters
    if (template->parameters) {
        for (size_t i = 0; i < template->parameter_count; i++) {
            free(template->parameters[i].name);
            free(template->parameters[i].description);
        }
        free(template->parameters);
    }

    free(template);
}

int mcp_resource_template_add_parameter(mcp_resource_template_t *template,
                                        const char *name,
                                        const char *description,
                                        int required) {
    if (!template || !name) {
        return -1;
    }

    // Reallocate parameters array
    size_t new_count = template->parameter_count + 1;
    mcp_resource_template_param_t *new_params = realloc(template->parameters,
                                                        new_count * sizeof(mcp_resource_template_param_t));
    if (!new_params) {
        return -1;
    }

    template->parameters = new_params;
    mcp_resource_template_param_t *param = &template->parameters[template->parameter_count];

    param->name = strdup(name);
    param->description = description ? strdup(description) : NULL;
    param->required = required;

    if (!param->name) {
        return -1;
    }

    template->parameter_count = new_count;
    return 0;
}

void mcp_resource_template_set_handler(mcp_resource_template_t *template,
                                       int (*handler)(const mcp_resource_template_context_t *context,
                                                      mcp_resource_content_t *content),
                                       void *user_data) {
    if (!template) return;
    
    template->handler = handler;
    template->user_data = user_data;
}

// =============================================================================
// URI Template Parsing - Simplified implementation
// =============================================================================

int mcp_resource_template_parse_uri(const char *uri_template,
                                    const char *resolved_uri,
                                    char ***param_names,
                                    char ***param_values,
                                    size_t *param_count) {
    if (!uri_template || !resolved_uri || !param_names || !param_values || !param_count) {
        return -1;
    }

    *param_names = NULL;
    *param_values = NULL;
    *param_count = 0;

    // For simplicity, we only support one {param} at the end of the template
    // This covers the most common case: file:///{path}
    const char *param_start = strstr(uri_template, "{");
    if (!param_start) {
        return 0; // No parameters
    }
    
    const char *param_end = strchr(param_start, '}');
    if (!param_end) {
        return -1; // Malformed template
    }
    
    // Check if there's anything after the parameter (not supported in simple mode)
    if (*(param_end + 1) != '\0') {
        return -1; // Complex templates not supported
    }
    
    // Extract parameter name
    size_t param_name_len = param_end - param_start - 1;
    char *param_name = malloc(param_name_len + 1);
    if (!param_name) return -1;
    
    strncpy(param_name, param_start + 1, param_name_len);
    param_name[param_name_len] = '\0';
    
    // Extract parameter value (everything after the prefix)
    size_t prefix_len = param_start - uri_template;
    if (strncmp(uri_template, resolved_uri, prefix_len) != 0) {
        free(param_name);
        return -1; // URI doesn't match template prefix
    }
    
    const char *param_value = resolved_uri + prefix_len;
    char *value_copy = strdup(param_value);
    if (!value_copy) {
        free(param_name);
        return -1;
    }
    
    // Allocate result arrays
    char **names = malloc(sizeof(char*));
    char **values = malloc(sizeof(char*));
    if (!names || !values) {
        free(param_name);
        free(value_copy);
        free(names);
        free(values);
        return -1;
    }
    
    names[0] = param_name;
    values[0] = value_copy;
    
    *param_names = names;
    *param_values = values;
    *param_count = 1;
    
    return 0;
}

int mcp_resource_template_matches_uri(const char *uri_template, const char *uri) {
    if (!uri_template || !uri) {
        return 0;
    }

    char **param_names = NULL;
    char **param_values = NULL;
    size_t param_count = 0;

    int result = mcp_resource_template_parse_uri(uri_template, uri, 
                                                 &param_names, &param_values, &param_count);
    
    // Clean up
    if (param_names) {
        for (size_t i = 0; i < param_count; i++) {
            free(param_names[i]);
        }
        free(param_names);
    }
    if (param_values) {
        for (size_t i = 0; i < param_count; i++) {
            free(param_values[i]);
        }
        free(param_values);
    }

    return (result == 0) ? 1 : 0;
}

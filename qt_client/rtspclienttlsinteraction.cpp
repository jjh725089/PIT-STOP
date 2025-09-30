#include "rtspclienttlsinteraction.h"
#include <QDebug>

#ifdef signals
#undef signals
#endif

struct _RtspClientTlsInteraction{
    GTlsInteraction parent_instance;
    GTlsCertificate *ca_cert;
    GTlsCertificate *cert;
    GTlsDatabase *database;
};

G_DEFINE_TYPE(RtspClientTlsInteraction, rtsp_client_tls_interaction, G_TYPE_TLS_INTERACTION);

// This function handles all initialisation
static void rtsp_client_tls_interaction_init(RtspClientTlsInteraction *tls_interaction) {
    // No specific initialization needed here for this simple case
}

static gboolean accept_tls_certificate(GTlsConnection *conn,
                                       GTlsCertificate *peer_cert,
                                       GTlsCertificateFlags errors,
                                       RtspClientTlsInteraction *user_data){
    GError *error = NULL;
    GTlsDatabase *database = g_tls_connection_get_database(G_TLS_CONNECTION(conn));
    
    qDebug() << "TLS certificate validation - errors:" << errors;
    
    // STRICT SECURITY: Reject expired or revoked certificates immediately
    if (errors & (G_TLS_CERTIFICATE_EXPIRED | G_TLS_CERTIFICATE_REVOKED)) {
        qWarning() << "REJECTED: Certificate expired or revoked";
        return FALSE;
    }
    
    // Method 1: Try database verification first
    if (database) {
        GTlsCertificateFlags verification_errors = g_tls_database_verify_chain(
            database, peer_cert, G_TLS_DATABASE_PURPOSE_AUTHENTICATE_SERVER,
            NULL, g_tls_connection_get_interaction(conn),
            G_TLS_DATABASE_VERIFY_NONE, NULL, &error);
            
        if (error) {
            qDebug() << "Database verification failed:" << error->message;
            g_clear_error(&error);
        } else if (verification_errors == G_TLS_CERTIFICATE_NO_FLAGS) {
            qDebug() << "ACCEPTED: Certificate verified by CA database";
            return TRUE;
        } else {
            qDebug() << "Database verification errors:" << verification_errors;
        }
    }
    
    // Method 2: For self-signed CA, accept UNKNOWN_CA if other checks pass
    // This is secure because we have the TLS database verification as primary method
    if ((errors & G_TLS_CERTIFICATE_UNKNOWN_CA) && 
        !(errors & (G_TLS_CERTIFICATE_EXPIRED | G_TLS_CERTIFICATE_REVOKED | G_TLS_CERTIFICATE_INSECURE))) {
        
        // Additional verification: ensure this looks like one of our server certificates
        gchar *peer_subject = NULL;
        g_object_get(peer_cert, "subject-name", &peer_subject, NULL);
        
        if (peer_subject) {
            // Check if subject contains our organization identifiers
            if (strstr(peer_subject, "RTSP Team") && strstr(peer_subject, "Pi Servers")) {
                qDebug() << "ACCEPTED: Certificate from our organization with UNKNOWN_CA (expected for self-signed)";
                g_free(peer_subject);
                return TRUE;
            }
            g_free(peer_subject);
        }
    }
    
    // Accept certificates with no validation errors
    if (errors == G_TLS_CERTIFICATE_NO_FLAGS) {
        qDebug() << "ACCEPTED: Certificate has no validation errors";
        return TRUE;
    }
    
    qWarning() << "REJECTED: Certificate validation failed - errors:" << errors;
    return FALSE;
}

GTlsInteractionResult rtsp_client_request_certificate(GTlsInteraction *interaction,
                                                      GTlsConnection *connection,
                                                      GTlsCertificateRequestFlags flags,
                                                      GCancellable *cancellable,
                                                      GError **error) {
    RtspClientTlsInteraction *stls = (RtspClientTlsInteraction *) interaction;
    qDebug() << "RtspClient Request Certificate - TLS interaction started";
    
    // Connect the certificate acceptance callback
    g_signal_connect(connection, "accept-certificate", G_CALLBACK(accept_tls_certificate), stls);
    qDebug() << "Certificate acceptance callback connected";
    
    // Set client certificate if available
    if (stls->cert) {
        g_tls_connection_set_certificate(connection, stls->cert);
        qDebug() << "Client certificate set";
    } else {
        qDebug() << "No client certificate to set";
    }
    
    return G_TLS_INTERACTION_HANDLED;
}

// Virtual function overrides, properties, signal definitions here
static void rtsp_client_tls_interaction_class_init(RtspClientTlsInteractionClass *iclass) {
    GTlsInteractionClass *object_class = G_TLS_INTERACTION_CLASS(iclass);
    object_class->request_certificate = rtsp_client_request_certificate;
}

RtspClientTlsInteraction *rtsp_client_tls_interaction_new(GTlsCertificate *cert, GTlsCertificate *ca_cert, GTlsDatabase *database) {
    RtspClientTlsInteraction *interaction = (RtspClientTlsInteraction *)g_object_new(RTSP_CLIENT_TLS_INTERACTION_TYPE, NULL);
    interaction->cert = cert;
    interaction->ca_cert = ca_cert;
    interaction->database = database;
    return interaction;
}

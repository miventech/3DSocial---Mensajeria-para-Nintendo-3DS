#include "AppState.hpp"

// Convierte la selección táctil o con cruceta en acciones del chat; los índices
// corresponden a los controles dibujados por Screens.cpp.
static void ejecutarAccionChat(int accion, u64& ultimoCheckTiempo) {
    switch (accion) {
    case 0: {
        std::string prompt = "Mensaje para " + contactoActivoNombre;
        std::string textoMsg = abrirTeclado(prompt.c_str());
        hidScanInput();
        if (textoMsg.empty()) break;
        long ultimoIdAntes = g_historialV2.empty() ? 0 : g_historialV2.back().id;
        historialChat.push_back("[Yo]: " + textoMsg);
        bool enviado = enviarMensajeServidor(miID.c_str(), contactoActivoID.c_str(), textoMsg.c_str());
        if (enviado) {
            if (g_servidorV2) cargarPaginaV2(0);
            else guardarHistorialEnSD(contactoActivoID);
            g_estadoEnvio = ENVIO_EXITO;
            if (g_servidorV2 && !g_historialV2.empty() &&
                g_historialV2.back().id > ultimoIdAntes) {
                g_ultimoMensajeAnimadoId = g_historialV2.back().id;
                g_ultimoMensajeAnimadoIndice = -1;
                g_ultimoMensajeAnimacion = osGetTime();
            } else if (!g_servidorV2) {
                g_ultimoMensajeAnimadoIndice = (int)historialChat.size() - 1;
                g_ultimoMensajeAnimadoId = 0;
                g_ultimoMensajeAnimacion = osGetTime();
            }
        } else {
            historialChat.pop_back();
            g_estadoEnvio = ENVIO_ERROR;
        }
        g_tiempoNotificacion = osGetTime();
        ultimoCheckTiempo = osGetTime();
        break;
    }
    case 1:
        if (g_audioPendiente) enviarNotaPendiente();
        else if (g_servidorV2 && !notaVozActiva())
            g_audioError = !iniciarNotaVoz(g_rutaAudioPendiente);
        else g_audioError = true;
        break;
    case 2:
        if (g_servidorV2) revisarFotoPendiente();
        else g_fotoError = true;
        break;
    case 3:
        estadoActual = g_pantallaRetornoChat;
        ultimoCheckTiempo = osGetTime();
        break;
    case 4: cargarMensajesAnteriores(); break;
    case 5: g_desplazamientoChat = std::max(0, g_desplazamientoChat - 1); break;
    case 6: seleccionarAudio(-1); break;
    case 7: g_audioError = !reproducirAudioSeleccionado(); break;
    case 8: seleccionarAudio(1); break;
    case 9: seleccionarFoto(-1); break;
    case 10: if (!mostrarFotoSeleccionada()) g_fotoError = true; break;
    case 11: seleccionarFoto(1); break;
    case 12:
        if (g_pantallaRetornoChat == PANTALLA_EXTRANOS) {
            guardarContactoSD(contactoActivoID, contactoActivoNombre);
            g_pantallaRetornoChat = PANTALLA_CONTACTOS;
            verificarMensajesNuevos();
        } else g_confirmarBorrarContacto = true;
        break;
    }
}

static void abrirConversacion(const std::string& id, const std::string& nombre,
                              EstadoApp retorno, int slotMiniatura, u64& ultimoCheckTiempo) {
    // Prepara la caché y la selección de medios igual para contactos y extraños.
    if (contactoActivoID != id) {
        contactoActivoID = id;
        contactoActivoNombre = nombre;
        cargarChatDesdeSD(id);
    }
    g_pantallaRetornoChat = retorno;
    g_v2Determinado = false;
    g_servidorV2 = false;
    g_masAntiguos = true;
    g_desplazamientoChat = 0;
    g_idsMensajesVisibles.clear();
    g_audioSeleccionadoId = 0;
    g_fotoSeleccionadaId = 0;
    cargarHistorialV2();
    seleccionarAudio(0);
    seleccionarFoto(0);
    g_rutaAudioPendiente = "sdmc:/3ds/Mensinger3DS/pendiente_" +
        claveServidorCache() + "_" + id + ".wav";
    FILE* pendiente = fopen(g_rutaAudioPendiente.c_str(), "rb");
    g_audioPendiente = pendiente != NULL;
    if (pendiente) fclose(pendiente);
    g_rutaFotoPendiente = "sdmc:/3ds/Mensinger3DS/pendiente_foto_" +
        claveServidorCache() + "_" + miID + "_" + id + ".bgr";
    pendiente = fopen(g_rutaFotoPendiente.c_str(), "rb");
    g_fotoPendiente = pendiente != NULL;
    if (pendiente) fclose(pendiente);
    g_slotMiniaturaChat = slotMiniatura;
    apagarLED();
    estadoActual = PANTALLA_CHAT;
    focoChat = 0;
    g_chatMedios = false;
    g_ultimoMensajeAnimadoId = 0;
    g_ultimoMensajeAnimadoIndice = -1;
    g_ultimoMensajeAnimacion = 0;
    actualizarChatDesdeServidor(miID.c_str(), id.c_str());
    g_ultimoMensajeAnimadoId = 0;
    g_ultimoMensajeAnimadoIndice = -1;
    g_ultimoMensajeAnimacion = 0;
    ultimoCheckTiempo = osGetTime();
}

bool procesarEntrada(u32 kDown, u32 kNav, u32 kUp, touchPosition touch, u64& ultimoCheckTiempo) {
        // Despacha cada fotograma tras la pantalla de inicio: kDown son pulsos,
        // kNav también repite navegación y kUp detecta al soltar la grabación.
        if (kDown & KEY_START) return false;
        actualizarReproduccionVoz();
        if (!notaVozActiva() && estadoActual != PANTALLA_CAMARA &&
            estadoActual != PANTALLA_CONFIRMAR_FOTO) actualizarPresencias();
        bool canceloGrabacion = false;
        if (estadoActual == PANTALLA_CHAT && notaVozActiva()) {
            bool puedeContinuar = continuarNotaVoz();
            if (kDown & KEY_B) {
                detenerNotaVoz(false);
                canceloGrabacion = true;
            } else if ((kUp & (KEY_TOUCH | KEY_Y)) ||
                       ((kUp & KEY_A) && focoChat == 1) || !puedeContinuar) {
                int duracion = detenerNotaVoz(true);
                if (duracion > 0) {
                    g_audioPendiente = true;
                    enviarNotaPendiente();
                } else g_audioError = true;
            }
        }

        if ((kDown & KEY_SELECT) && (estadoActual == PANTALLA_CONTACTOS ||
                                     estadoActual == PANTALLA_CHAT) && !notaVozActiva()) {
            pantallaRetornoConfig = estadoActual;
            g_pestanaConfig = std::min(1, g_pestanaConfig);
            focoConfiguracion = 0;
            estadoActual = PANTALLA_CONFIGURACION;
            kDown = 0;
            touch.px = 0; touch.py = 0;
        }

        // Selección visible; la lista solo se desplaza al cruzar el borde.
        if (estadoActual == PANTALLA_CONTACTOS) {
            if (listaContactos.empty()) contactoSeleccionado = scrollContactos = 0;
            else {
                contactoSeleccionado = std::max(0, std::min(contactoSeleccionado,
                    (int)listaContactos.size() - 1));
                if ((kNav & KEY_DDOWN) && contactoSeleccionado + 1 < (int)listaContactos.size())
                    ++contactoSeleccionado;
                if ((kNav & KEY_DUP) && contactoSeleccionado > 0)
                    --contactoSeleccionado;
                if (contactoSeleccionado < scrollContactos) scrollContactos = contactoSeleccionado;
                if (contactoSeleccionado >= scrollContactos + 3)
                    scrollContactos = contactoSeleccionado - 2;
            }
            if ((kDown & KEY_A) && !listaContactos.empty()) {
                touch.px = 80;
                touch.py = 37 + (contactoSeleccionado - scrollContactos) * 50 + 22;
                kDown |= KEY_TOUCH;
            } else if (kDown & KEY_X) {
                touch.px = 40; touch.py = 210;
                kDown |= KEY_TOUCH;
            } else if (kDown & KEY_L) {
                abrirPerfil();
                kDown = 0;
            } else if (kDown & KEY_Y) {
                abrirPerfilContacto(contactoSeleccionado);
                kDown = 0;
            }
            actualizarMiniaturasContactos();
        }

        if (estadoActual == PANTALLA_PERFIL) {
            if (g_perfilPropio && (kNav & KEY_DDOWN) && focoPerfil < 13) ++focoPerfil;
            if (g_perfilPropio && (kNav & KEY_DUP) && focoPerfil > 0) --focoPerfil;
            if (g_perfilPropio && focoPerfil < g_scrollPerfil) g_scrollPerfil = focoPerfil;
            if (g_perfilPropio && focoPerfil >= g_scrollPerfil + 7) g_scrollPerfil = focoPerfil - 6;
            if ((kDown & KEY_B) || (!g_perfilPropio && (kDown & KEY_A))) {
                cerrarFotoVista();
                g_perfilFotoCargada = false;
                estadoActual = PANTALLA_CONTACTOS;
                kDown = 0;
            } else if (g_perfilPropio && (kDown & KEY_X) && focoPerfil < 4) {
                std::string* campos[] = {&g_perfilNombre, &g_perfilApodo, &g_perfilBio, &g_perfilDescripcion};
                campos[focoPerfil]->clear();
                guardarAjustesSD();
                g_perfilError = !guardarPerfilServidor();
            } else if (g_perfilPropio && (kDown & KEY_X) && focoPerfil == 6) {
                if (abrirTeclado("Escribe QUITAR para borrar clave") == "QUITAR")
                    g_perfilError = !cambiarContrasena("");
            } else if (g_perfilPropio && (kDown & KEY_A)) {
                touch.px = 145;
                touch.py = 8 + (focoPerfil - g_scrollPerfil) * 29 + 13;
                kDown |= KEY_TOUCH;
            }
        }

        if (estadoActual == PANTALLA_BUSQUEDA || estadoActual == PANTALLA_CONOCER) {
            if (kDown & KEY_B) { estadoActual = PANTALLA_CONTACTOS; kDown = 0; }
            else if (kDown & KEY_L) {
                estadoActual = PANTALLA_BUSQUEDA; g_consultaBusqueda.clear(); buscarPerfiles();
            }
            else if (kDown & KEY_R) {
                estadoActual = PANTALLA_CONOCER; g_consultaBusqueda.clear(); buscarPerfiles();
            }
            else if ((kNav & KEY_DDOWN) && g_scrollBusqueda + 3 < (int)g_resultadosBusqueda.size())
                ++g_scrollBusqueda;
            else if ((kNav & KEY_DUP) && g_scrollBusqueda > 0) --g_scrollBusqueda;
            else if (kDown & KEY_X) { touch.px = 40; touch.py = 223; kDown |= KEY_TOUCH; }
        }

        if (estadoActual == PANTALLA_EXTRANOS) {
            if (kDown & KEY_B) { estadoActual = PANTALLA_CONTACTOS; kDown = 0; }
            else if ((kNav & KEY_DDOWN) && g_seleccionDesconocidos + 1 < (int)g_desconocidos.size())
                ++g_seleccionDesconocidos;
            else if ((kNav & KEY_DUP) && g_seleccionDesconocidos > 0)
                --g_seleccionDesconocidos;
            g_scrollDesconocidos = std::max(0, std::min(g_scrollDesconocidos, g_seleccionDesconocidos));
            if (g_seleccionDesconocidos >= g_scrollDesconocidos + 3)
                g_scrollDesconocidos = g_seleccionDesconocidos - 2;
            if ((kDown & KEY_A) && !g_desconocidos.empty()) {
                touch.px = 50; touch.py = 38 + (g_seleccionDesconocidos - g_scrollDesconocidos) * 50;
                kDown |= KEY_TOUCH;
            } else if ((kDown & KEY_X) && !g_desconocidos.empty()) {
                touch.px = 240; touch.py = 38 + (g_seleccionDesconocidos - g_scrollDesconocidos) * 50;
                kDown |= KEY_TOUCH;
            }
        }

        if (estadoActual == PANTALLA_CHAT && !g_confirmarBorrarContacto && !notaVozActiva()) {
            if (!g_chatMedios) {
                if (kNav & KEY_DLEFT) {
                    if (focoChat == 1) focoChat = 0;
                    else if (focoChat == 4) focoChat = 2;
                    else if (focoChat == 5) focoChat = 4;
                }
                if (kNav & KEY_DRIGHT) {
                    if (focoChat == 0) focoChat = 1;
                    else if (focoChat == 2) focoChat = 4;
                    else if (focoChat == 4) focoChat = 5;
                }
                if (kNav & KEY_DDOWN) {
                    if (focoChat == 0) focoChat = 2;
                    else if (focoChat == 1) focoChat = 5;
                    else if (focoChat == 2 || focoChat == 4 || focoChat == 5) focoChat = 3;
                }
                if (kNav & KEY_DUP) {
                    if (focoChat == 3) focoChat = 2;
                    else if (focoChat == 2 || focoChat == 4) focoChat = 0;
                    else if (focoChat == 5) focoChat = 1;
                }
            } else {
                if ((kNav & KEY_DLEFT) && (focoChat == 7 || focoChat == 8 ||
                    focoChat == 10 || focoChat == 11)) --focoChat;
                if ((kNav & KEY_DRIGHT) && (focoChat == 6 || focoChat == 7 ||
                    focoChat == 9 || focoChat == 10)) ++focoChat;
                if ((kNav & KEY_DDOWN) && focoChat >= 6 && focoChat <= 8) focoChat += 3;
                else if ((kNav & KEY_DDOWN) && focoChat >= 9 && focoChat <= 11) focoChat = 12;
                if ((kNav & KEY_DUP) && focoChat == 12) focoChat = 9;
                else if ((kNav & KEY_DUP) && focoChat >= 9 && focoChat <= 11) focoChat -= 3;
            }
            if (kDown & (KEY_A | KEY_X | KEY_Y | KEY_L | KEY_R)) {
                int accion = (kDown & KEY_A) ? focoChat : (kDown & KEY_X) ? 2 :
                             (kDown & KEY_Y) ? 1 : (kDown & KEY_L) ? 4 : 5;
                if (accion < 6) { g_chatMedios = false; focoChat = accion; }
                touch.px = CHAT_FOCO_X[accion] + CHAT_FOCO_ANCHO[accion] / 2;
                touch.py = CHAT_FOCO_Y[accion] + CHAT_FOCO_ALTO[accion] / 2;
                kDown |= KEY_TOUCH;
            }
        }
        if (estadoActual == PANTALLA_CHAT && g_confirmarBorrarContacto && (kDown & KEY_A)) {
            touch.px = 230; touch.py = 165;
            kDown |= KEY_TOUCH;
        }

        if (estadoActual == PANTALLA_CONFIGURACION) {
            int ultimoFoco = g_pestanaConfig == 0 ? 4 : 6;
            if (kDown & (KEY_L | KEY_R)) { g_pestanaConfig = 1 - g_pestanaConfig; focoConfiguracion = 0; }
            if ((kNav & KEY_DDOWN) && focoConfiguracion < ultimoFoco) ++focoConfiguracion;
            if ((kNav & KEY_DUP) && focoConfiguracion > 0) --focoConfiguracion;
            if (kDown & (KEY_B | KEY_SELECT | KEY_X)) {
                guardarAjustesSD();
                estadoActual = pantallaRetornoConfig;
                kDown = 0;
            } else if (kDown & KEY_A) {
                touch.px = (g_pestanaConfig == 1 && focoConfiguracion == 5) ? 230 : 80;
                touch.py = g_pestanaConfig == 0 ? (focoConfiguracion < 4 ? 60 + focoConfiguracion * 36 : 218) :
                    focoConfiguracion < 4 ? 58 + focoConfiguracion * 31 :
                    focoConfiguracion == 4 || focoConfiguracion == 5 ? 182 : 218;
                kDown |= KEY_TOUCH;
            }
        }

        // Comprobar mensajes nuevos periódicamente
        if (estadoActual == PANTALLA_CONTACTOS || estadoActual == PANTALLA_EXTRANOS) {
            if (osGetTime() - ultimoCheckTiempo > (u64)g_intervaloConsultaMs) {
                verificarMensajesNuevos();
                ultimoCheckTiempo = osGetTime();
            }
            if (g_ultimaCargaPerfiles == 0 || osGetTime() - g_ultimaCargaPerfiles > 30000)
                actualizarPerfilesContactos();
        }

        if (estadoActual == PANTALLA_CHAT && !notaVozActiva() &&
            osGetTime() - ultimoCheckTiempo > (u64)g_intervaloConsultaMs) {
            actualizarChatDesdeServidor(miID.c_str(), contactoActivoID.c_str());
            verificarMensajesNuevos();
            ultimoCheckTiempo = osGetTime();
        }

        // --- NAVEGACIÓN CON BOTONES FÍSICOS ---
        if (estadoActual == PANTALLA_FOTO && (kDown & (KEY_A | KEY_B))) {
            cerrarFotoVista();
            estadoActual = PANTALLA_CHAT;
        }
        else if (estadoActual == PANTALLA_CAMARA) {
            if (kDown & KEY_B) cerrarCapturaFoto(false);
            else if (kDown & KEY_A) tomarFotoVista();
        }
        else if (estadoActual == PANTALLA_CONFIRMAR_FOTO) {
            if (kDown & KEY_B) cerrarCapturaFoto(true);
            else if (kDown & KEY_X) repetirFotoVista();
            else if (kDown & KEY_A) confirmarEnvioFoto();
        }
        else if (estadoActual == PANTALLA_CHAT && (kDown & KEY_B) && !canceloGrabacion) {
            if (g_confirmarBorrarContacto) {
                g_confirmarBorrarContacto = false;
            } else if (g_chatMedios) {
                g_chatMedios = false;
                focoChat = 0;
            } else if (g_audioPendiente) {
                remove(g_rutaAudioPendiente.c_str());
                g_audioPendiente = false;
                g_audioError = false;
            } else if (g_fotoPendiente) {
                remove(g_rutaFotoPendiente.c_str());
                g_fotoPendiente = false;
                g_fotoError = false;
            } else {
                estadoActual = g_pantallaRetornoChat;
                touch.px = 0; touch.py = 0;
            }
        }

        // --- INTERACCIÓN TÁCTIL (USANDO SWITCH) ---
        if ((kDown & KEY_TOUCH) && touch.px > 0 && touch.py > 0) {

            switch (estadoActual) {

            // Lista de contactos
            case PANTALLA_CONTACTOS: {
                // Barra inferior: ajustes solo mediante SELECT en la pantalla superior.
                if (touch.py >= 192 && touch.py <= 231) {
                    if (touch.px >= 10 && touch.px <= 106) {
                        std::string nuevaID = abrirTeclado(getText(TXT_PASO_1));
                        hidScanInput();
                        if (!nuevaID.empty() && nuevaID.length() <= 10) {
                            bool esNum = true;
                            for (char c : nuevaID) if (!isdigit(c)) esNum = false;
                            if (esNum) {
                                std::string nuevoNombre = abrirTeclado(getText(TXT_PASO_2));
                                guardarContactoSD(nuevaID, nuevoNombre);
                                for (size_t i = 0; i < listaContactos.size(); i++) {
                                    if (listaContactos[i].id == nuevaID) {
                                        contactoSeleccionado = (int)i;
                                        scrollContactos = std::max(0, contactoSeleccionado - 2);
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    else if (touch.px >= 83 && touch.px <= 158) {
                        estadoActual = PANTALLA_BUSQUEDA;
                        g_consultaBusqueda.clear();
                        buscarPerfiles();
                    }
                    else if (touch.px >= 162 && touch.px <= 234) {
                        estadoActual = PANTALLA_CONOCER;
                        g_consultaBusqueda.clear();
                        buscarPerfiles();
                    }
                    else if (touch.px >= 238 && touch.px <= 310) {
                        verificarMensajesNuevos();
                        estadoActual = PANTALLA_EXTRANOS;
                    }
                }
                // Scroll de la lista
                else if (touch.px >= 278 && touch.py >= 37 && touch.py < 177) {
                    if (touch.py < 107 && scrollContactos > 0) {
                        scrollContactos--;
                    } else if (touch.py >= 107 && scrollContactos + 3 < (int)listaContactos.size()) {
                        scrollContactos++;
                    }
                    contactoSeleccionado = std::max(scrollContactos,
                        std::min(contactoSeleccionado, scrollContactos + 2));
                }
                // Seleccionar Contacto
                else if (touch.px >= 8 && touch.px < 272 && touch.py >= 37 && touch.py < 181 && !listaContactos.empty()) {
                    int index = scrollContactos + ((touch.py - 37) / 50);
                    if (index >= 0 && index < (int)listaContactos.size()) {
                        contactoSeleccionado = index;
                        std::string nuevoID = listaContactos[index].id;

                        listaContactos[index].tieneNovedad = false;
                        listaContactos[index].pendientes = 0;
                        abrirConversacion(nuevoID, listaContactos[index].usuarioPerfil.empty() ?
                            listaContactos[index].nombre : listaContactos[index].usuarioPerfil,
                            PANTALLA_CONTACTOS,
                            listaContactos[index].tieneMiniatura ? index - scrollContactos : -1,
                            ultimoCheckTiempo);
                    }
                }
            break;
        }

        case PANTALLA_EXTRANOS: {
            if (touch.py >= 192) {
                if (touch.px < 115) estadoActual = PANTALLA_CONTACTOS;
                else if (touch.px >= 250 && g_scrollDesconocidos + 3 < (int)g_desconocidos.size())
                    ++g_scrollDesconocidos;
                else if (g_scrollDesconocidos > 0) --g_scrollDesconocidos;
            } else if (touch.py >= 37 && touch.py < 181 && touch.px >= 8 && touch.px < 310) {
                int indice = g_scrollDesconocidos + (touch.py - 37) / 50;
                if (indice >= 0 && indice < (int)g_desconocidos.size()) {
                    g_seleccionDesconocidos = indice;
                    RemitenteDesconocido item = g_desconocidos[indice];
                    if (touch.px >= 213) {
                        guardarContactoSD(item.id, "Usuario " + item.id);
                        verificarMensajesNuevos();
                        estadoActual = PANTALLA_CONTACTOS;
                    } else {
                        abrirConversacion(item.id, "Usuario " + item.id,
                            PANTALLA_EXTRANOS, -1, ultimoCheckTiempo);
                        verificarMensajesNuevos();
                    }
                }
            }
            break;
        }

        // 3. PANTALLA CHAT
        case PANTALLA_CHAT: {
            if (g_confirmarBorrarContacto) {
                if (touch.py >= 148 && touch.py <= 190 && touch.px >= 20 && touch.px <= 154) {
                    g_confirmarBorrarContacto = false;
                } else if (touch.py >= 148 && touch.py <= 190 && touch.px >= 166 && touch.px <= 300) {
                    borrarContactoActual(contactoActivoID);
                    historialChat.clear();
                    g_historialV2.clear();
                    cerrarFotoVista();
                    g_perfilFotoCargada = false;
                    contactoActivoID.clear();
                    contactoActivoNombre.clear();
                    g_confirmarBorrarContacto = false;
                    if (scrollContactos > 0 && scrollContactos + 3 > (int)listaContactos.size())
                        scrollContactos = std::max(0, (int)listaContactos.size() - 3);
                    contactoSeleccionado = listaContactos.empty() ? 0 :
                        std::min(contactoSeleccionado, (int)listaContactos.size() - 1);
                    estadoActual = PANTALLA_CONTACTOS;
                    ultimoCheckTiempo = osGetTime();
                }
                touch.px = 0; touch.py = 0;
                break;
            }
            if (touch.py >= 5 && touch.py < 40) {
                g_chatMedios = touch.px >= 160;
                focoChat = g_chatMedios ? 7 : 0;
                break;
            }
            int inicio = g_chatMedios ? 6 : 0;
            int fin = g_chatMedios ? 13 : 6;
            for (int i = inicio; i < fin; ++i) {
                if (touch.px >= CHAT_FOCO_X[i] &&
                    touch.px < CHAT_FOCO_X[i] + CHAT_FOCO_ANCHO[i] &&
                    touch.py >= CHAT_FOCO_Y[i] &&
                    touch.py < CHAT_FOCO_Y[i] + CHAT_FOCO_ALTO[i]) {
                    focoChat = i;
                    ejecutarAccionChat(i, ultimoCheckTiempo);
                    break;
                }
            }
            break;
        }
        case PANTALLA_CAMARA:
            if (touch.py >= 90 && touch.py <= 169 && touch.px >= 35 && touch.px <= 285)
                tomarFotoVista();
            else if (touch.py >= 175 && touch.py <= 235 && touch.px >= 55 && touch.px <= 265)
                cerrarCapturaFoto(false);
            touch.px = 0; touch.py = 0;
            break;

        case PANTALLA_CONFIRMAR_FOTO:
            if (touch.py >= 90 && touch.py <= 175 && touch.px >= 15 && touch.px <= 155)
                repetirFotoVista();
            else if (touch.py >= 90 && touch.py <= 175 && touch.px >= 165 && touch.px <= 305)
                confirmarEnvioFoto();
            else if (touch.py >= 180 && touch.py <= 235 && touch.px >= 55 && touch.px <= 265)
                cerrarCapturaFoto(true);
            touch.px = 0; touch.py = 0;
            break;

        case PANTALLA_FOTO:
            if (touch.px > 0 || touch.py > 0) {
                cerrarFotoVista();
                estadoActual = PANTALLA_CHAT;
            }
            touch.px = 0; touch.py = 0;
            break;

        case PANTALLA_BUSQUEDA:
        case PANTALLA_CONOCER: {
            if (touch.py >= 7 && touch.py < 172) {
                int indice = g_scrollBusqueda + (touch.py - 7) / 55;
                if (indice >= 0 && indice < (int)g_resultadosBusqueda.size() && touch.px >= 225) {
                    const ResultadoBusqueda& item = g_resultadosBusqueda[indice];
                    guardarContactoSD(item.id, item.nombre.empty() ? item.apodo : item.nombre);
                }
            } else if (touch.py >= 179 && touch.py <= 205) {
                if (touch.px < 86) estadoActual = PANTALLA_CONTACTOS;
                else {
                    estadoActual = touch.px >= 198 ? PANTALLA_CONOCER : PANTALLA_BUSQUEDA;
                    g_consultaBusqueda.clear();
                    buscarPerfiles();
                }
            } else if (touch.py > 205 && touch.px < 120) {
                std::string consulta = abrirTeclado(estadoActual == PANTALLA_CONOCER ?
                    "Buscar en Conocer gente" : "Buscar por nombre");
                if (!consulta.empty()) {
                    g_consultaBusqueda = limitarTextoUtf8(consulta, 32);
                    buscarPerfiles();
                }
            } else if (touch.py > 205) {
                if (touch.px > 270 && g_scrollBusqueda + 3 < (int)g_resultadosBusqueda.size())
                    ++g_scrollBusqueda;
                else if (g_scrollBusqueda > 0) --g_scrollBusqueda;
            }
            break;
        }

        case PANTALLA_PERFIL: {
            if (!g_perfilPropio && (touch.px > 0 || touch.py > 0)) {
                cerrarFotoVista();
                g_perfilFotoCargada = false;
                estadoActual = PANTALLA_CONTACTOS;
            } else if (g_perfilPropio && touch.py >= 212 && touch.px >= 170 && touch.px <= 245) {
                if (touch.px < 208) g_scrollPerfil = std::max(0, g_scrollPerfil - 1);
                else g_scrollPerfil = std::min(7, g_scrollPerfil + 1);
                focoPerfil = std::max(g_scrollPerfil, std::min(focoPerfil, g_scrollPerfil + 6));
            } else if (g_perfilPropio && touch.py >= 8 && touch.py < 211) {
                int fila = g_scrollPerfil + (touch.py - 8) / 29;
                focoPerfil = std::max(0, std::min(13, fila));
                if (fila < 4) {
                    const char* titulos[] = {"Nombre de usuario", "Apodo", "Biografia", "Descripcion"};
                    std::string* campos[] = {&g_perfilNombre, &g_perfilApodo, &g_perfilBio, &g_perfilDescripcion};
                    std::string nuevo = abrirTeclado(titulos[fila]);
                    hidScanInput();
                    if (!nuevo.empty()) {
                        size_t limite = fila < 2 ? 32 : 120;
                        if (nuevo.size() > limite) nuevo.resize(limite);
                        for (char& c : nuevo) if ((unsigned char)c < 32) c = ' ';
                        *campos[fila] = nuevo;
                        guardarAjustesSD();
                        g_perfilError = !guardarPerfilServidor();
                    }
                } else if (fila == 4 || fila == 5) {
                    if (fila == 4) g_mostrarEnBusqueda = !g_mostrarEnBusqueda;
                    else g_quieroConocerGente = !g_quieroConocerGente;
                    guardarAjustesSD();
                    g_perfilError = !guardarPerfilServidor();
                } else if (fila == 6) {
                    std::string nueva = abrirTecladoContrasena("Nueva contrasena");
                    if (!nueva.empty()) g_perfilError = !cambiarContrasena(nueva);
                } else if (fila == 7) {
                    cerrarFotoVista();
                    g_fotoPerfilEnCurso = true;
                    g_pantallaRetornoFotoPerfil = PANTALLA_PERFIL;
                    abrirCamaraFoto();
                } else if (fila >= 8 && fila <= 12) {
                    int* colores[] = {&g_colorChat, &g_colorFondo, &g_colorTexto,
                        &g_colorNombre, &g_colorApodo};
                    int* color = colores[fila - 8];
                    *color = (*color + 1) % 6;
                    guardarAjustesSD();
                    g_perfilError = !guardarPerfilServidor();
                } else {
                    guardarAjustesSD();
                    g_perfilError = !sincronizarPerfilServidor();
                }
            }
            touch.px = 0; touch.py = 0;
            break;
        }

        // 4. PANTALLA CONFIGURACIÓN
        case PANTALLA_CONFIGURACION: {
            if (touch.py >= 8 && touch.py <= 36) {
                g_pestanaConfig = touch.px >= 160 ? 1 : 0;
                g_errorConfig = false;
                focoConfiguracion = 0;
            }
            else if (g_pestanaConfig == 0 && touch.py >= 44 && touch.py <= 181) {
                int fila = (touch.py - 44) / 36;
                focoConfiguracion = fila;
                if (fila == 0) g_modoOscuro = !g_modoOscuro;
                else if (fila == 1) g_ledActivado = !g_ledActivado;
                else if (fila == 2) g_sonidoActivado = !g_sonidoActivado;
                else if (fila == 3) g_buscarActualizaciones = !g_buscarActualizaciones;
            }
            else if (g_pestanaConfig == 1 && touch.py >= 44 && touch.py <= 73) {
                focoConfiguracion = 0;
                std::string nuevoHost = abrirTeclado(getText(TXT_SERVIDOR));
                if (!nuevoHost.empty()) {
                    if (servidorValido(nuevoHost)) { g_servidorHost = nuevoHost; g_errorConfig = false; g_estadoPruebaConexion = 0; }
                    else g_errorConfig = true;
                }
            }
            else if (g_pestanaConfig == 1 && touch.py >= 75 && touch.py <= 104) {
                focoConfiguracion = 1;
                std::string nuevoPuerto = abrirTeclado(getText(TXT_PUERTO));
                if (!nuevoPuerto.empty()) {
                    char* fin = NULL;
                    long puerto = strtol(nuevoPuerto.c_str(), &fin, 10);
                    if (fin && *fin == '\0' && puerto >= 1 && puerto <= 65535) {
                        g_servidorPuerto = (int)puerto; g_errorConfig = false; g_estadoPruebaConexion = 0;
                    } else g_errorConfig = true;
                }
            }
            else if (g_pestanaConfig == 1 && touch.py >= 106 && touch.py <= 135) {
                focoConfiguracion = 2;
                g_intervaloConsultaMs = g_intervaloConsultaMs == 1000 ? 2000 :
                    g_intervaloConsultaMs == 2000 ? 5000 : g_intervaloConsultaMs == 5000 ? 10000 : 1000;
            }
            else if (g_pestanaConfig == 1 && touch.py >= 137 && touch.py <= 166) {
                focoConfiguracion = 3;
                g_cifrarConexion = !g_cifrarConexion;
                g_estadoPruebaConexion = 0;
                g_claveServidorError = false;
            }
            else if (g_pestanaConfig == 1 && touch.py >= 169 && touch.py <= 196) {
                focoConfiguracion = touch.px < 160 ? 4 : 5;
                if (touch.px < 160) {
                    RespuestaHttp pruebaTls;
                    g_estadoPruebaConexion = pedirHttp("GET", "/version", NULL, "",
                        pruebaTls, 128) && pruebaTls.estado == 200 ? 1 : -1;
                } else {
                    g_servidorHost = SERVIDOR_PREDETERMINADO;
                    g_servidorPuerto = PUERTO_PREDETERMINADO;
                    g_intervaloConsultaMs = 2000;
                    g_cifrarConexion = true;
                    g_estadoPruebaConexion = 0;
                    g_errorConfig = false;
                }
            }
            else if (touch.py >= 204 && touch.py <= 235) {
                focoConfiguracion = g_pestanaConfig == 0 ? 4 : 6;
                guardarAjustesSD();
                estadoActual = pantallaRetornoConfig;
            }
            touch.px = 0; touch.py = 0;
            break;
        }
    }
}
    return true;
}

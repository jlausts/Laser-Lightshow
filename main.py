import streamlit as st
import socket
import json

# -- Config --
LASER_HOST = '127.0.0.1'
LASER_PORT = 65432

def send_to_laser_server(x_pairs, y_pairs, rgb):
    msg = json.dumps({
        'x': x_pairs,
        'y': y_pairs,
        'rgb': rgb
    }).encode('utf-8')
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.connect((LASER_HOST, LASER_PORT))
            s.sendall(msg)
    except Exception as e:
        st.error(f"Socket error: {e}")

# -- State setup --
st.set_page_config(layout='wide')
ss = st.session_state

# Defaults
if 'x_freqs' not in ss: ss.x_freqs = [200.0, 400.0, 600.0, 800.0]
if 'x_amps' not in ss: ss.x_amps = [0.0, 0.0, 0.0, 0.0]
if 'y_freqs' not in ss: ss.y_freqs = [300.0, 500.0, 700.0, 900.0]
if 'y_amps' not in ss: ss.y_amps = [0.0, 0.0, 0.0, 0.0]
if 'r' not in ss: ss.r = 128
if 'g' not in ss: ss.g = 128
if 'b' not in ss: ss.b = 128

# Cache previous state to detect changes
if 'prev' not in ss:
    ss.prev = {
        'x': list(zip(ss.x_freqs, ss.x_amps)),
        'y': list(zip(ss.y_freqs, ss.y_amps)),
        'rgb': [ss.r, ss.g, ss.b]
    }

# -- Layout --
rgb, xy = st.columns([1, 1])

# -- RGB --
with rgb:
    st.subheader("RGB")
    ss.r = st.slider("Red", 32, 255, ss.r)
    ss.g = st.slider("Green", 32, 255, ss.g)
    ss.b = st.slider("Blue", 32, 255, ss.b)

# -- Waveform pairs --
with xy:
    st.subheader("Waveform Pairs (X + Y)")
    for i in range(4):
        with st.expander(f"Wave Pair {i+1}", expanded=True):
            col1, col2 = st.columns(2)
            with col1:
                ss.x_freqs[i] = st.number_input(f"X{i+1} Freq", value=ss.x_freqs[i], key=f"x_freq_{i}")
                ss.x_amps[i] = st.slider(f"X{i+1} Amp", 0.0, 1.0, ss.x_amps[i], key=f"x_amp_{i}")
            with col2:
                ss.y_freqs[i] = st.number_input(f"Y{i+1} Freq", value=ss.y_freqs[i], key=f"y_freq_{i}")
                ss.y_amps[i] = st.slider(f"Y{i+1} Amp", 0.0, 1.0, ss.y_amps[i], key=f"y_amp_{i}")

# -- Auto-send on change --
current_x = list(zip(ss.x_freqs, ss.x_amps))
current_y = list(zip(ss.y_freqs, ss.y_amps))
current_rgb = [ss.r, ss.g, ss.b]

if (current_x != ss.prev['x'] or current_y != ss.prev['y'] or current_rgb != ss.prev['rgb']):
    send_to_laser_server(current_x, current_y, current_rgb)
    ss.prev['x'] = current_x
    ss.prev['y'] = current_y
    ss.prev['rgb'] = current_rgb
    st.caption("🎯 Sent updated data to laser.")

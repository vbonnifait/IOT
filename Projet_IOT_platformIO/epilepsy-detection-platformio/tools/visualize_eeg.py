#!/usr/bin/env python3
"""
Real-Time EEG Signal Visualization
Visualise le signal EEG depuis l'ESP32 en temps réel
"""

import serial
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from collections import deque
import argparse
import re

class EEGVisualizer:
    def __init__(self, port, baudrate=115200, window_size=500):
        """
        Initialise le visualiseur
        
        Args:
            port: Port série (ex: 'COM3', '/dev/ttyUSB0')
            baudrate: Vitesse de communication
            window_size: Nombre d'échantillons à afficher
        """
        self.port = port
        self.baudrate = baudrate
        self.window_size = window_size
        
        # Buffers pour les données
        self.time_buffer = deque(maxlen=window_size)
        self.raw_buffer = deque(maxlen=window_size)
        self.filtered_buffer = deque(maxlen=window_size)
        self.probability_buffer = deque(maxlen=100)
        
        # Connexion série
        try:
            self.serial = serial.Serial(port, baudrate, timeout=1)
            print(f"✓ Connecté à {port} @ {baudrate} baud")
        except Exception as e:
            print(f"❌ Erreur de connexion: {e}")
            raise
        
        # Configuration du graphique
        self.setup_plot()
        
        # Compteurs
        self.sample_count = 0
        self.seizure_count = 0
        
    def setup_plot(self):
        """Configure les graphiques matplotlib"""
        self.fig, self.axes = plt.subplots(3, 1, figsize=(12, 8))
        self.fig.suptitle('Visualisation EEG en Temps Réel - BITalino + ESP32', 
                         fontsize=14, fontweight='bold')
        
        # Graphique 1: Signal brut
        self.axes[0].set_title('Signal EEG Brut')
        self.axes[0].set_ylabel('Amplitude (µV)')
        self.axes[0].grid(True, alpha=0.3)
        self.line_raw, = self.axes[0].plot([], [], 'b-', linewidth=0.8, label='Brut')
        self.axes[0].legend(loc='upper right')
        
        # Graphique 2: Signal filtré
        self.axes[1].set_title('Signal EEG Filtré (0.5-40 Hz)')
        self.axes[1].set_ylabel('Amplitude (µV)')
        self.axes[1].grid(True, alpha=0.3)
        self.line_filtered, = self.axes[1].plot([], [], 'g-', linewidth=0.8, label='Filtré')
        self.axes[1].legend(loc='upper right')
        
        # Graphique 3: Probabilité de crise
        self.axes[2].set_title('Probabilité de Crise')
        self.axes[2].set_xlabel('Temps (échantillons)')
        self.axes[2].set_ylabel('Probabilité')
        self.axes[2].set_ylim(0, 1)
        self.axes[2].grid(True, alpha=0.3)
        self.line_prob, = self.axes[2].plot([], [], 'r-', linewidth=1.5, label='P(crise)')
        self.axes[2].axhline(y=0.7, color='orange', linestyle='--', 
                            linewidth=1, label='Seuil (70%)')
        self.axes[2].legend(loc='upper right')
        
        # Zone de détection
        self.axes[2].axhspan(0.7, 1.0, alpha=0.2, color='red')
        
        # Informations
        self.info_text = self.fig.text(0.02, 0.02, '', fontsize=10, 
                                       family='monospace')
        
        plt.tight_layout()
    
    def parse_line(self, line):
        """
        Parse une ligne du port série
        
        Format attendu:
        - "ADC: 512, Raw: 100.5, Filtered: 95.3"
        - "P(crise)=0.85"
        - "ALERTE CRISE"
        """
        data = {}
        
        # Chercher les valeurs numériques
        adc_match = re.search(r'ADC\s*[=:]\s*(\d+)', line)
        if adc_match:
            data['adc'] = int(adc_match.group(1))
        
        raw_match = re.search(r'Raw\s*[=:]\s*([-+]?\d+\.?\d*)', line)
        if raw_match:
            data['raw'] = float(raw_match.group(1))
        
        filtered_match = re.search(r'Filtered\s*[=:]\s*([-+]?\d+\.?\d*)', line)
        if filtered_match:
            data['filtered'] = float(filtered_match.group(1))
        
        prob_match = re.search(r'P\(crise\)\s*[=:]\s*(\d+\.?\d*)%?', line)
        if prob_match:
            prob = float(prob_match.group(1))
            # Si en pourcentage
            if '%' in line or prob > 1:
                prob = prob / 100.0
            data['probability'] = prob
        
        # Détecter les alertes
        if 'ALERTE' in line or 'SEIZURE' in line:
            data['alert'] = True
            self.seizure_count += 1
        
        return data
    
    def update_plot(self, frame):
        """Mise à jour des graphiques (appelé par FuncAnimation)"""
        # Lire les données disponibles
        while self.serial.in_waiting > 0:
            try:
                line = self.serial.readline().decode('utf-8', errors='ignore').strip()
                
                if line:
                    data = self.parse_line(line)
                    
                    # Mettre à jour les buffers
                    if 'raw' in data or 'filtered' in data:
                        self.sample_count += 1
                        self.time_buffer.append(self.sample_count)
                        
                        if 'raw' in data:
                            self.raw_buffer.append(data['raw'])
                        else:
                            self.raw_buffer.append(self.raw_buffer[-1] if self.raw_buffer else 0)
                        
                        if 'filtered' in data:
                            self.filtered_buffer.append(data['filtered'])
                        else:
                            self.filtered_buffer.append(self.filtered_buffer[-1] if self.filtered_buffer else 0)
                    
                    if 'probability' in data:
                        self.probability_buffer.append(data['probability'])
                    
            except Exception as e:
                print(f"⚠️  Erreur de lecture: {e}")
                continue
        
        # Mettre à jour les graphiques si on a des données
        if len(self.time_buffer) > 0:
            # Signal brut
            self.line_raw.set_data(list(self.time_buffer), list(self.raw_buffer))
            self.axes[0].relim()
            self.axes[0].autoscale_view()
            
            # Signal filtré
            self.line_filtered.set_data(list(self.time_buffer), list(self.filtered_buffer))
            self.axes[1].relim()
            self.axes[1].autoscale_view()
        
        # Probabilité
        if len(self.probability_buffer) > 0:
            prob_time = range(len(self.probability_buffer))
            self.line_prob.set_data(prob_time, list(self.probability_buffer))
            self.axes[2].set_xlim(0, max(100, len(self.probability_buffer)))
        
        # Mettre à jour les informations
        current_prob = self.probability_buffer[-1] if self.probability_buffer else 0
        status = "⚠️ CRISE" if current_prob > 0.7 else "✓ Normal"
        
        info = f"Échantillons: {self.sample_count:6d} | "
        info += f"État: {status:15s} | "
        info += f"P(crise): {current_prob*100:5.1f}% | "
        info += f"Crises détectées: {self.seizure_count}"
        
        self.info_text.set_text(info)
        
        return self.line_raw, self.line_filtered, self.line_prob, self.info_text
    
    def run(self):
        """Lance la visualisation"""
        print("\n" + "="*60)
        print("VISUALISATION EEG EN TEMPS RÉEL")
        print("="*60)
        print(f"Port: {self.port}")
        print(f"Baudrate: {self.baudrate}")
        print(f"Taille de fenêtre: {self.window_size} échantillons")
        print("\nAppuyez sur Ctrl+C pour arrêter...")
        print("="*60 + "\n")
        
        # Lancer l'animation
        anim = FuncAnimation(self.fig, self.update_plot, 
                           interval=50, blit=True, cache_frame_data=False)
        
        try:
            plt.show()
        except KeyboardInterrupt:
            print("\n\n✓ Arrêt de la visualisation")
        finally:
            self.serial.close()
            print("✓ Port série fermé")

def main():
    parser = argparse.ArgumentParser(
        description='Visualisation en temps réel du signal EEG depuis ESP32'
    )
    parser.add_argument('port', 
                       help='Port série (ex: COM3, /dev/ttyUSB0)')
    parser.add_argument('-b', '--baudrate', 
                       type=int, default=115200,
                       help='Vitesse de communication (défaut: 115200)')
    parser.add_argument('-w', '--window', 
                       type=int, default=500,
                       help='Taille de la fenêtre (défaut: 500)')
    
    args = parser.parse_args()
    
    # Créer et lancer le visualiseur
    visualizer = EEGVisualizer(args.port, args.baudrate, args.window)
    visualizer.run()

if __name__ == '__main__':
    main()

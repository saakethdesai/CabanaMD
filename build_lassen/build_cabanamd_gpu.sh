export KOKKOS_SRC_DIR=/g/g90/desai7/Codes/kokkos
export BUILD_DIR=build_lassen
export KOKKOS_INSTALL_DIR=$KOKKOS_SRC_DIR/$BUILD_DIR/install
export CABANA_INSTALL_DIR=/g/g90/desai7/Codes/Cabana_pr/$BUILD_DIR/install  # try make a good guess
export N2P2_INSTALL_DIR=/g/g90/desai7/Codes/n2p2_lassen

cmake \
  -DCMAKE_BUILD_TYPE="Release" \
  -D KOKKOS_DIR=$KOKKOS_INSTALL_DIR \
  -D CABANA_DIR=$CABANA_INSTALL_DIR \
  -D CabanaMD_ENABLE_Serial=OFF \
  -D CabanaMD_ENABLE_OpenMP=OFF \
  -D CabanaMD_ENABLE_Cuda=ON \
  -D N2P2_DIR=$N2P2_INSTALL_DIR \
  -D CabanaMD_ENABLE_NNP=ON \
  -D CMAKE_CXX_COMPILER=$KOKKOS_SRC_DIR/bin/nvcc_wrapper \
  -DCMAKE_CXX_FLAGS="-std=c++11" \
  ..; 
make install 

#cmake -DCMAKE_BUILD_TYPE=Debug -D KOKKOS_DIR=$KOKKOS_INSTALL_DIR -D CABANA_DIR=$CABANA_INSTALL_DIR -D CabanaMD_ENABLE_Serial=ON -D CabanaMD_ENABLE_OpenMP=OFF -D CabanaMD_ENABLE_Cuda=OFF -DCMAKE_CXX_FLAGS="-Wall -Wextra -pedantic -Werror" ..
#  -DCMAKE_CXX_FLAGS="-std=c++11" \

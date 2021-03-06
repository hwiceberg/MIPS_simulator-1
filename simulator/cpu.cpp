#include "cpu.h"
#include <iostream>

CPU::CPU(MemSys* memsys, FPU* fpu = nullptr, VU* vu = nullptr){
  _memsys = memsys;
  _fpu = fpu;
  _vu = vu;
  pc = 0;
  clk = 0;
  status = 0;
  err = false;
  for(int i=0; i<16; ++i) {
    gpr[i] = 0;
    fpr[i] = 0.0;
    vr[i] = 0;
  }
  for(int i=0; i<5; ++i) {
    pipe[i] = nullptr;
  }
  clear = true;
  piped = true;
}

void CPU::ifc() {
  if (pipe[0] == nullptr && (clear || piped)) {
    pipe[0] = new Instruction();
    if (piped) {
      if (pipe[3]!= nullptr && pipe[3]->type == 3 && pipe[3]->cond) {
        pc = pipe[3]->aluoutput;
      }
    }
    pipe[0]->add = pc;
    pipe[0]->stage = 0;
    pipe[0]->npc = pc + 4;
    if (piped) {
      pc = pc + 4;
    }
    pipe[0]->dst = -1;
    clear = false;
    std::cout<<"fectch instructions at "<<pipe[0]->add<<std::endl;
  }
  if (pipe[0] != nullptr) {
    if (pipe[0]->stage == 0) {
      int flag = _memsys->loadWord(pipe[0]->add, &(pipe[0]->ins));
      if (flag == -1) {
        std::cout<<"error fectching instructions"<<std::endl;
        err = true;
        return;
      } else if (flag == 1) {
        pipe[0]->stage = 1;
        std::cout<<"instruction fectched "<<pipe[0]->ins<<std::endl;
      }
    }
    if (pipe[0]->stage == 1) {
      if (pipe[1] == nullptr) {
        pipe[1] = pipe[0];
        pipe[0] = nullptr;
      }
    } else if (pipe[0]->stage > 1) {
      std::cout<<"ifc: error instruction stage"<<std::endl;
      err = true;
    }
  }
  return;
}

void CPU::idc() {
  if (pipe[1] != nullptr) {
    if (pipe[1]->stage == 1) {
      uint32_t ins = pipe[1]->ins;
      pipe[1]->imm = ins & 0x1ffff;
      if ((pipe[1]->imm) >> 16 == 1) {//sign-extended
        pipe[1]->imm |= 0xfffe0000;
      }
      ins = ins >> 13;
      pipe[1]->rd3 = ins & 0xf;
      ins = ins >> 4;
      pipe[1]->rd2 = ins & 0xf;
      ins = ins >> 4;
      pipe[1]->rd1 = ins & 0xf;
      ins = ins >> 4;
      pipe[1]->opcode = ins;
      ins = ins >> 4;
      switch(ins) {
        case 1: pipe[1]->type = 1;
                break;
        case 4:
        case 5: pipe[1]->type = 2;
                break;
        case 0: pipe[1]->type = 3;
                break;
        case 2: pipe[1]->type = 4;
                break;
        case 3: pipe[1]->type = 5;
                break;
        case 6:
        case 7: pipe[1]->type = 6;
                break;
      }
      if (pipe[1]->type == 2 || pipe[1]->type == 6) {//alu, simd
        pipe[1]->opcode = pipe[1]->opcode & 0x1f;
      } else {
        pipe[1]->opcode = pipe[1]->opcode & 0xf;
      }
      bool ready = false;
      if (pipe[1]->type == 4) {
        if (pipe[1]->opcode == 5) {
            ready = opReady(pipe[1]->rd1, 1);
            //pipe[1]->A = gpr[pipe[1]->rd1];
        } else if (pipe[1]->opcode == 6){
            //pipe[1]->fA = fpr[pipe[1]->rd1];
            ready = opReady(16+pipe[1]->rd1, 1);
        } else {
          bool tmp1 = opReady(16+pipe[1]->rd1, 1);
          bool tmp2 = opReady(16+pipe[1]->rd2, 2);

          //   pipe[1]->fA = fpr[pipe[1]->rd1];
          //   pipe[1]->fB = fpr[pipe[1]->rd2];

          ready = tmp1 && tmp2;
        }
      } else if (pipe[1]->type == 6) {
        //std::cout<<pipe[1]->rd1<<" "<<pipe[1]->rd2<<std::endl;
        if (pipe[1]->opcode < 10){
          bool tmp1 = opReady(32+pipe[1]->rd1, 1);
          bool tmp2 = opReady(32+pipe[1]->rd2, 2);

          //   pipe[1]->vA = vr[pipe[1]->rd1];
          //   pipe[1]->vB = vr[pipe[1]->rd2];

          ready = tmp1 && tmp2;
        } else if (pipe[1]->opcode == 13 ) {
          bool tmp1 = opReady(pipe[1]->rd1, 1);
          bool tmp2 = opReady(32+pipe[1]->rd2, 2);
          ready = tmp1 && tmp2;

        } else if ( pipe[1]->opcode == 14) {
          //std::cout<<pipe[1]->rd1<<std::endl;
            //pipe[1]->A = gpr[pipe[1]->rd1];
            ready = opReady(pipe[1]->rd1, 1);
        } else if (pipe[1]->opcode == 10 || pipe[1]->opcode == 11 || pipe[1]->opcode == 12) {
            //pipe[1]->vA = vr[pipe[1]->rd1];
          ready = opReady(32+pipe[1]->rd1, 1);
        }
//         if (pipe[1]->opcode < 10) {
//           pipe[1]->vA = vr[pipe[1]->rd1];
//           pipe[2]->vB = vr[pipe[1]->rd2];
//         } else if (pipe[1]->opcode <13) {
//           pipe[1]->vA = vr[pipe[1]->rd1];
//         } else if (pipe[1]->opcode == 13 || pipe[1]->opcode == 14) {
//           pipe[1]->A = gpr[pipe[1]->rd1];
//           pipe[1]->vB = vr[pipe[1]->rd2];
//         }
      } else if (pipe[1]->type == 1) {
        if (pipe[1]->opcode < 8) {
          //  pipe[1]->A = gpr[pipe[1]->rd1];
          ready = opReady(pipe[1]->rd1, 1);
        } else {
          bool tmp1 = opReady(pipe[1]->rd1, 1);
          bool tmp2 = opReady(pipe[1]->rd2, 2);
          // if (tmp1) {
          //   pipe[1]->A = gpr[pipe[1]->rd1];
          // }
          // if (tmp2) {
          //   pipe[1]->B = gpr[pipe[1]->rd2];
          // }
          ready = tmp1 && tmp2;
        }
      } else if (pipe[1]->type == 2) {
        if (pipe[1]->opcode < 19 && pipe[1]->opcode != 11) {
          bool tmp1 = opReady(pipe[1]->rd1, 1);
          bool tmp2 = opReady(pipe[1]->rd2, 2);
          // if (tmp1) {
          //   pipe[1]->A = gpr[pipe[1]->rd1];
          // }
          // if (tmp2) {
          //   pipe[1]->B = gpr[pipe[1]->rd2];
          // }
          ready = tmp1 && tmp2;
        } else {
          //pipe[1]->A = gpr[pipe[1]->rd1];
          ready = opReady(pipe[1]->rd1, 1);
        }
      } else if (pipe[1]->type == 3) {
        if (pipe[1]->opcode < 3) {
          ready = true;
        } else if (pipe[1]->opcode == 3 || pipe[1]->opcode == 4) {
          bool tmp1 = opReady(pipe[1]->rd1, 1);
          bool tmp2 = opReady(pipe[1]->rd2, 2);
          // if (tmp1) {
          //   pipe[1]->A = gpr[pipe[1]->rd1];
          // }
          // if (tmp2) {
          //   pipe[1]->B = gpr[pipe[1]->rd2];
          // }
          ready = tmp1 && tmp2;
        } else if (pipe[1]->opcode > 4 && pipe[1]->opcode < 9) {
          //pipe[1]->A = gpr[pipe[1]->rd1];
          ready = opReady(pipe[1]->rd1, 1);
        }
      } else if (pipe[1]->type == 5){
          ready = opReady(pipe[1]->rd1, 1);
      }

      if (ready) {
        std::cout<<"idc: type:"<<pipe[1]->type<<" opcode: "<<pipe[1]->opcode<<" rd1: "
        <<pipe[1]->rd1<<" rd2: "<<pipe[1]->rd2<<" rd3: "<<pipe[1]->rd3<<" imm: "<<pipe[1]->imm<<std::endl;
        pipe[1]->stage = 2;
      }
    }
    if (pipe[1]->stage == 2) {
      if (pipe[2] == nullptr) {
        pipe[2] = pipe[1];
        pipe[1] = nullptr;
      }
    } else if (pipe[1]->stage > 2) {
      std::cout<<"idc: error instruction stage"<<std::endl;
      err = true;
    }
  }
  return;
}

void CPU::exc() {
  if (pipe[2] != nullptr) {
    if (pipe[2]->stage == 2) {
      if (pipe[2]->type == 1) {
        pipe[2]->aluoutput = pipe[2]->A + pipe[2]->imm;
        if (pipe[2]->opcode == 0 || pipe[2]->opcode == 1 || pipe[2]->opcode == 2) {
          pipe[2]->dst = pipe[2]->rd2;
        } else if (pipe[2]->opcode == 4) {
          pipe[2]->dst = 16 + pipe[2]->rd2;
        }
        std::cout<<"exc: "<<pipe[2]->aluoutput<<std::endl;
        pipe[2]->stage = 3;

      } else if (pipe[2]->type == 2) {
        if (pipe[2]->opcode < 19) {
          pipe[2]->dst = pipe[2]->rd3;
        } else {
          pipe[2]->dst = pipe[2]->rd2;
        }
        switch (pipe[2]->opcode) {
          case 0: {pipe[2]->aluoutput = pipe[2]->A + pipe[2]->B;
                   uint64_t tmp = (uint64_t)pipe[2]->A + (uint64_t)pipe[2]->B;
                   status = (tmp>>32) & 0x1;}//status = 1
                   break;
          case 1: {pipe[2]->aluoutput = pipe[2]->A - pipe[2]->B;
                   status = ((int)(pipe[2]->A < pipe[2]->B)>>1);} //status = 2
                   break;
          case 2: {int64_t tmp = ((int32_t)pipe[2]->A) * ((int32_t)pipe[2]->B);
                  pipe[2]->aluoutput = tmp & 0xffffffff;}
                  break;
          case 3: {int64_t tmp = ((int32_t)pipe[2]->A) * ((int32_t)pipe[2]->B);
                  pipe[2]->aluoutput = tmp>>32;}
                  break;
          case 4: {uint64_t tmp =  pipe[2]->A * pipe[2]->B;
                  pipe[2]->aluoutput = tmp & 0xffffffff;}
                  break;
          case 5: {uint64_t tmp = pipe[2]->A * pipe[2]->B;
                  pipe[2]->aluoutput = tmp>>32;}
                  break;
          case 6: pipe[2]->aluoutput = ((int32_t)pipe[2]->A) / ((int32_t)pipe[2]->B);
                  break;
          case 7: pipe[2]->aluoutput = pipe[2]->A / pipe[2]->B;
                  break;
          case 8: pipe[2]->aluoutput = pipe[2]->A % pipe[2]->B;
                  break;
          case 9: pipe[2]->aluoutput = pipe[2]->A & pipe[2]->B;
                  break;
          case 10: pipe[2]->aluoutput = pipe[2]->A | pipe[2]->B;
                  break;
          case 11: pipe[2]->aluoutput = ~pipe[2]->A;
                  break;
          case 12: pipe[2]->aluoutput = pipe[2]->A ^ pipe[2]->B;
                  break;
          case 13:{uint32_t tmp = 0; uint32_t tA = pipe[2]->A;
                  for(uint32_t i=0; i<pipe[2]->B; ++i) {
                    tmp <<= 1; tmp |= tA & 1; tA >>= 1;
                  }
                  pipe[2]->aluoutput = tA | (tmp<<(32-pipe[2]->B));}
                  break;
          case 14: pipe[2]->aluoutput = pipe[2]->A >> pipe[2]->B;
                  break;
          case 15: pipe[2]->aluoutput = (int32_t)pipe[2]->A >> pipe[2]->B;
                  break;
          case 16: pipe[2]->aluoutput = pipe[2]->A << pipe[2]->B;
                  break;
          case 17: pipe[2]->aluoutput = (int32_t)pipe[2]->A < (int32_t)pipe[2]->B;
                  break;
          case 18: pipe[2]->aluoutput = pipe[2]->A < pipe[2]->B;
                  break;
          case 19: {pipe[2]->aluoutput = pipe[2]->A + pipe[2]->imm;
                    int64_t tmp = (int64_t)pipe[2]->A + (int64_t)pipe[2]->imm;
                    status = (tmp>>32) & 0x1;} // status = 1
                    break;
          case 20: {pipe[2]->aluoutput = pipe[2]->A - pipe[2]->imm;
                    int64_t tmp = (int64_t)pipe[2]->A - (int64_t)pipe[2]->imm;
                    status = (tmp>>32) & 0x1;
                    status = status*2;} // status = 2
                    break;
          case 21: pipe[2]->aluoutput = (int32_t)pipe[2]->A < (int32_t)pipe[2]->imm;
                  break;
          case 22: pipe[2]->aluoutput = pipe[2]->A < pipe[2]->imm;
                  break;
          default: std::cout<<"exc: type 2 error opcode "<<pipe[2]->opcode<<std::endl;
                return;
        }
        std::cout<<"exc: "<<pipe[2]->aluoutput<<std::endl;
        pipe[2]->stage = 3;
      } else if (pipe[2]->type == 3) {
        if (pipe[2]->opcode == 0) {
          // err = true;
          // delete pipe[2];  //TODO clear pipe or not?
          // pipe[2] = nullptr;
          // std::cout<<"break"<<std::endl;
          // return;
        } else if (pipe[2]->opcode < 3) {
          uint32_t offset = pipe[2]->ins & 0x1ffffff;
          if (offset >> 24 == 1) {
            offset |= 0xfe000000;
          }
          pipe[2]->aluoutput = offset<<2;
          if (pipe[2]->opcode == 2) {
            gpr[15] = pipe[2]->npc;
          }
          pipe[2]->cond = true;
        } else {
          pipe[2]->aluoutput = pipe[2]->npc + (pipe[2]->imm<<2);
          switch(pipe[2]->opcode) {
            case 3: pipe[2]->cond = (pipe[2]->A == pipe[2]->B);
                  break;
            case 4: pipe[2]->cond = (pipe[2]->A != pipe[2]->B);
                  break;
            case 5: pipe[2]->cond = ((int32_t)pipe[2]->A >= 0);
                  break;
            case 6: pipe[2]->cond = ((int32_t)pipe[2]->A > 0);
                  break;
            case 7: pipe[2]->cond = ((int32_t)pipe[2]->A <= 0);
                  break;
            case 8: pipe[2]->cond = ((int32_t)pipe[2]->A < 0);
                  break;
          }
        }
        if (pipe[2]->cond) {
          std::cout<<"branch taken"<<std::endl;
          if (pipe[1] != nullptr) {
              delete pipe[1];
              pipe[1] = nullptr;
          } else if (pipe[0] != nullptr) {
              if (_memsys->isBusy()) {
                std::string r = "lw" + std::to_string(pipe[0]->add);
                if (r == _memsys->getRequest()) {
                  _memsys->clear();
                }
              }
              delete pipe[0];
              pipe[0] = nullptr;
          }
        }
        std::cout<<"exc: "<<pipe[2]->aluoutput<<std::endl;
        pipe[2]->stage = 3;
      } else if (pipe[2]->type == 4) {
        if (pipe[2]->opcode < 4) {
          pipe[2]->dst = 16 + pipe[2]->rd3;
          int flag = _fpu->fpuCal(pipe[2]->fA, pipe[2]->fB, &pipe[2]->fpuoutput, pipe[2]->opcode);
          if (flag == 1) {
            std::cout<<"exc: "<<pipe[2]->fpuoutput<<std::endl;
            pipe[2]->stage = 3;
          } else if (flag == -1) {
            std::cout<<"exc: error fpuCal"<<std::endl;
            return;
          }
        } else {
          if (pipe[2]->opcode == 4) {
            pipe[2]->dst = 16 + pipe[2]->rd3;
            pipe[2]->aluoutput = pipe[2]->fA < pipe[2]->fB;
          } else if (pipe[2]->opcode == 5) {
            pipe[2]->dst = 16 + pipe[2]->rd2;
            float *tmp = (float*)(&pipe[2]->A);
            pipe[2]->fpuoutput = *tmp;
          } else if (pipe[2]->opcode == 6) {
            pipe[2]->dst = pipe[2]->rd2;
            uint32_t *tmp = (uint32_t*)(&pipe[2]->fA);
            pipe[2]->aluoutput = *tmp;
          } else {
            std::cout<<"exc: error opcode"<<std::endl;
            return;
          }
          std::cout<<"exc: "<<pipe[2]->aluoutput<<std::endl;
          pipe[2]->stage = 3;
        }
      } else if (pipe[2]->type == 5) {
        pipe[2]->aluoutput = pipe[2]->A + pipe[2]->imm;
        //pipe[2]->dst = pipe[2]->rd2;
        std::cout<<"exc: "<<pipe[2]->aluoutput<<std::endl;
        pipe[2]->stage = 3;
      } else if (pipe[2]->type == 6) {
        if (pipe[2]->opcode < 10) {
          pipe[2]->dst = 32 + pipe[2]->rd3;
          int flag = _vu->vuCal(pipe[2]->vA, pipe[2]->vB, &pipe[2]->vuoutput, pipe[2]->opcode);
          if (flag == 1) {
            std::cout<<"exc: "<<pipe[2]->vuoutput<<std::endl;
            pipe[2]->stage = 3;
          } else if (flag == -1) {
            std::cout<<"exc:: error vuCal"<<std::endl;
            return;
          }
        } else {
          if (pipe[2]->opcode == 10) {
            pipe[2]->dst = 32 + pipe[2]->rd2;
            pipe[2]->vuoutput = pipe[2]->vA;
          } else if (pipe[2]->opcode == 12) {
            pipe[2]->dst = pipe[2]->rd2;
            int n = 7 - pipe[2]->imm;
            uint32_t tmp = (pipe[2]->vA>>(8*n)) & 0xff;
            pipe[2]->aluoutput = tmp;
          } else if (pipe[2]->opcode == 11) {
            pipe[2]->dst = pipe[2]->rd2;
            uint64_t tmp = pipe[2]->vA;
            uint32_t sum = 0;
            for (int i = 0; i < 8; ++i) {
              sum += (tmp & 0xff);
              tmp >>= 8;
            }
            pipe[2]->aluoutput = sum;
          } else if (pipe[2]->opcode == 13) {
            pipe[2]->dst = 32 + pipe[2]->rd2;
            uint64_t tmp = pipe[2]->A & 0xff;
            int n = (7 - pipe[2]->imm)*8;
            uint64_t mask = 0xff;
            mask = ~(mask << n);
            pipe[2]->vuoutput = (pipe[2]->vB & mask) | (tmp<<n);
            std::cout<<"exc: mask:"<<mask<<" n:"<<n<<" tmp:"<<(tmp<<n)<<std::endl;
          } else if (pipe[2]->opcode == 14) {
            pipe[2]->dst = 32 + pipe[2]->rd2;
            uint64_t tmp = pipe[2]->A & 0xff;
            uint64_t res = 0;
            res = tmp;
            for (int i=1; i<8; ++i) {
              res <<=8;
              res |= tmp;
            }
            pipe[2]->vuoutput = res;
          }
          pipe[2]->stage = 3;
          std::cout<<"exc: "<<pipe[2]->vuoutput<<std::endl;
        }
      }
    }
    if (pipe[2]->stage == 3) {
      if (pipe[3] == nullptr) {
        pipe[3] = pipe[2];
        pipe[2] = nullptr;
      }
    } else if (pipe[2]->stage > 3) {
      std::cout<<"exc: error instruction stage"<<std::endl;
      err = true;
    }
  }
  return;
}

void CPU::mem() {
  if (pipe[3] != nullptr) {
    if (pipe[3]->stage == 3) {
      if (!piped) {
        pc = pipe[3]->npc;
      }
      if (pipe[3]->type == 1) {
        int flag = 0;
        if (pipe[3]->opcode == 8) {
          flag = _memsys->storeByte(pipe[3]->aluoutput, (uint8_t)(pipe[3]->B & 0xff));
          //std::cout<<"mem sb "<<flag<<" "<<(int)(pipe[3]->B & 0xff)<<std::endl;
        } else if (pipe[3]->opcode == 9) {
          flag = _memsys->storeWord(pipe[3]->aluoutput, (uint32_t)(pipe[3]->B));
        } else if (pipe[3]->opcode == 0 || pipe[3]->opcode == 1) {
          uint8_t tmp;
          flag = _memsys->loadByte(pipe[3]->aluoutput, &tmp);
          if (flag == 1) {
            pipe[3]->lmd = (uint32_t)tmp;
            if (pipe[3]->opcode == 0 && tmp >> 7 == 1) {
              pipe[3]->lmd |= 0xffffff00;
            }
          }
        } else if (pipe[3]->opcode == 2 || pipe[3]->opcode == 4) {
          uint32_t tmp;
          int flag = _memsys->loadWord(pipe[3]->aluoutput, &tmp);
          if (flag == 1) {
            pipe[3]->lmd = tmp;
          }
        }
        //std::cout<<"flag "<<flag<<std::endl;
        if (flag == 1 ) {
          pipe[3]->stage = 4;
          std::cout<<"pc: "<<pc<<std::endl;
        }
      } else if (pipe[3]->type == 2) {
        pipe[3]->stage = 4;
        std::cout<<"pc: "<<pc<<std::endl;
      } else if (pipe[3]->type == 3) {
        if (pipe[3]->opcode == 0) {
          err = true;
          delete pipe[3];  //TODO: clear pipe?
          pipe[3] = nullptr;
          std::cout<<"break"<<std::endl;
          return;
        }
        if (pipe[3]->cond) {
          if (!piped){
            pc = pipe[3]->aluoutput;
          }
        }
        pipe[3]->stage = 4;
        std::cout<<"pc: "<<pc<<std::endl;
      } else if (pipe[3]->type == 5) {
        uint32_t tmp;
        int flag = _memsys->loadWord(pipe[3]->aluoutput, &tmp);
        if (flag == 1 ) {
          pipe[3]->stage = 4;
          std::cout<<"pc: "<<pc<<std::endl;
        }
      } else {
        pipe[3]->stage = 4;
        std::cout<<"pc: "<<pc<<std::endl;
      }
    }
    if(pipe[3]->stage == 4) {
      if (pipe[4] == nullptr) {
        pipe[4] = pipe[3];
        pipe[3] = nullptr;
      }
    } else if (pipe[3]->stage > 4) {
      std::cout<<"mem: error instruction stage"<<std::endl;
      err = true;
    }
  }
  return;
}

void CPU::wbc() {
  if (pipe[4] != nullptr) {
    if (pipe[4]->stage == 4) {
      if (pipe[4]->type == 1) {
        if (pipe[4]->opcode == 0 || pipe[4]->opcode == 1 || pipe[4]->opcode == 2) {
          gpr[pipe[4]->rd2] = pipe[4]->lmd;
          emit gprNotify(pipe[4]->rd2, pipe[4]->lmd);
          std::cout<<"wbc: "<<gpr[pipe[4]->rd2]<<std::endl;
        } else if (pipe[4]->opcode == 4) {
          float *tmp = (float*)(&pipe[4]->lmd);
          fpr[pipe[4]->rd2] = *tmp;
          emit fprNotify(pipe[4]->rd2, *tmp);
          std::cout<<"wbc: "<<fpr[pipe[4]->rd2]<<std::endl;
        }
      } else if (pipe[4]->type == 2) {
        if (pipe[4]->opcode > 18) {
          gpr[pipe[4]->rd2] = pipe[4]->aluoutput;
          emit gprNotify(pipe[4]->rd2, pipe[4]->aluoutput);
          std::cout<<"wbc: "<<gpr[pipe[4]->rd2]<<std::endl;
        } else {
          gpr[pipe[4]->rd3] = pipe[4]->aluoutput;
          emit gprNotify(pipe[4]->rd3, pipe[4]->aluoutput);
          std::cout<<"wbc: "<<gpr[pipe[4]->rd3]<<std::endl;
        }
      } else if (pipe[4]->type == 4) {
        if (pipe[4]->opcode < 4) {
          fpr[pipe[4]->rd3] = pipe[4]->fpuoutput;
          emit fprNotify(pipe[4]->rd3, pipe[4]->fpuoutput);
        } else if (pipe[4]->opcode == 4) {
          gpr[pipe[4]->rd3] = pipe[4]->aluoutput;
          emit gprNotify(pipe[4]->rd3, pipe[4]->aluoutput);
        } else if (pipe[4]->opcode == 5) {
          fpr[pipe[4]->rd2] = pipe[4]->fpuoutput;
          emit fprNotify(pipe[4]->rd2, pipe[4]->fpuoutput);
        } else if (pipe[4]->opcode == 6) {
          gpr[pipe[4]->rd2] = pipe[4]->aluoutput;
          emit gprNotify(pipe[4]->rd2, pipe[4]->aluoutput);
        }
      } else if (pipe[4]->type == 6) {
        if (pipe[4]->opcode < 11) {
          vr[pipe[4]->rd3] = pipe[4]->vuoutput;
          emit vrNotify(pipe[4]->rd3, pipe[4]->vuoutput);
        } else if (pipe[4]->opcode == 11 || pipe[4]->opcode == 12) {
          gpr[pipe[4]->rd2] = pipe[4]->aluoutput;
          emit gprNotify(pipe[4]->rd2, pipe[4]->aluoutput);
        } else if (pipe[4]->opcode == 13 || pipe[4]->opcode == 14) {
          vr[pipe[4]->rd2] = pipe[4]->vuoutput;
          emit vrNotify(pipe[4]->rd2, pipe[4]->vuoutput);
        }
      }
      pipe[4]->stage = 5;
    }
    if(pipe[4]->stage == 5) {
      clear = true;
      delete pipe[4];
      pipe[4] = nullptr;
    } else if (pipe[4]->stage > 5){
      std::cout<<"wbc: error instruction stage"<<std::endl;
      err = true;
    }
  }
  return;
}

bool CPU::opReady(int rd, int a) {
    if (rd < 0) {
        return false;
    }
    if (pipe[2] != nullptr && pipe[2]->dst == rd) { //exc
      return false;
    }
    int type = rd/16;
    if (pipe[3] != nullptr && pipe[3]->dst == rd) { //mem
      if (pipe[3]->type == 1) {
        return false;
      } else {
        if (a == 1) {
          switch(type) {
            case 0: pipe[1]->A = pipe[3]->aluoutput;
                    break;
            case 1: pipe[1]->fA = pipe[3]->fpuoutput;
                    break;
            case 2: pipe[1]->vA = pipe[3]->vuoutput;
                    break;
          }
        } else {
          switch(type) {
            case 0: pipe[1]->B = pipe[3]->aluoutput;
                    break;
            case 1: pipe[1]->fB = pipe[3]->fpuoutput;
                    break;
            case 2: pipe[1]->vB = pipe[3]->vuoutput;
                    break;
          }
        }
        return true;
      }
    }
    if (pipe[4] != nullptr && pipe[4]->dst == rd) {
      if (pipe[4]->type == 1) {
        if (a == 1) {
          switch(type) {
            case 0: pipe[1]->A = pipe[4]->lmd;
                    break;
            case 1: {float *tmp = (float*)(&pipe[4]->lmd);
                    pipe[1]->fA = *tmp;
                    break;}
            case 2: {std::cout<<"opReady error: vu oprand"<<std::endl;
                    break;}
          }
        } else {
          switch(type) {
            case 0: pipe[1]->B = pipe[4]->lmd;
                    break;
            case 1: {float *tmp = (float*)(&pipe[4]->lmd);
                    pipe[1]->fB = *tmp;
                    break;}
            case 2: {std::cout<<"opReady error: vu oprand"<<std::endl;
                    break;}
          }
        }
        return true;
      } else {
        if (a == 1) {
          switch(type) {
            case 0: pipe[1]->A = pipe[4]->aluoutput;
                    break;
            case 1: pipe[1]->fA = pipe[4]->fpuoutput;
                    break;
            case 2: pipe[1]->vA = pipe[4]->vuoutput;
                    break;
          }
        } else {
          switch(type) {
            case 0: pipe[1]->B = pipe[4]->aluoutput;
                    break;
            case 1: pipe[1]->fB = pipe[4]->fpuoutput;
                    break;
            case 2: pipe[1]->vB = pipe[4]->vuoutput;
                    break;
          }
        }
        return true;
      }
    }

    if (a == 1) {
      switch(type) {
        case 0: pipe[1]->A = gpr[pipe[1]->rd1];
                break;
        case 1: pipe[1]->fA = fpr[pipe[1]->rd1];
                break;
        case 2: pipe[1]->vA = vr[pipe[1]->rd1];
                break;
      }
    } else {
      switch(type) {
        case 0: pipe[1]->B = gpr[pipe[1]->rd2];
                break;
        case 1: pipe[1]->fB = fpr[pipe[1]->rd2];
                break;
        case 2: pipe[1]->vB = vr[pipe[1]->rd2];
                break;
      }
    }
    return true;
}

void CPU::run() {
  while(!err) {
    wbc();
    mem();
    exc();
    idc();
    ifc();
    ++clk;
  }
}

void CPU::step() {
  if (err) {
    std::cout<<"error program"<<std::endl;
    return;
  }
  //std::cout<<"wbc"<<std::endl;
  wbc();
  //std::cout<<"mem"<<std::endl;
  mem();
  //std::cout<<"exc"<<std::endl;
  exc();
  //std::cout<<"idc"<<std::endl;
  idc();
  //std::cout<<"ifc"<<std::endl;
  ifc();
  std::cout<<"pipe"<<std::endl;
  for (int i = 0; i < 5; ++i) {
    if (pipe[i] != nullptr) {
      std::cout<<i<<" "<<pipe[i]->type<<" "<<pipe[i]->opcode<<std::endl;
    }
  }

  ++clk;
  return;
}

void CPU::reset() {
    clk = 0;
    err = false;
    clear = true;
    pc = 0;
    for (int i= 0; i < 5; ++i) {
        if (pipe[i] != nullptr) {
            delete pipe[i];
            pipe[i] = nullptr;
        }
    }
    for (int i = 0; i < 16; ++i) {
        gpr[i] = 0;
        fpr[i] = 0;
        vr[i] = 0;
    }
    _memsys->clear();
}
